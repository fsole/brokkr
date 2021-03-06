/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/mesh.h"
#include "core/maths.h"
#include "core/image.h"

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::core::maths;
using namespace bkk::framework;

class ambient_occlusion_sample_t : public application_t
{
public:
  ambient_occlusion_sample_t()
    :application_t("Screen-space ambient occlusion", 1200u, 800u, 3u),
    cameraController_(vec3(0.0f, 4.0f, 12.0f), vec2(0.1f, 0.0f), 0.5f, 0.01f),
    ssaoEnabled_(true),
    ssaoSampleCount_(64u),
    ssaoRadius_(0.5f),
    ssaoBias_(0.025f)
  {
    uvec2 imageSize(1200u, 800u);
    renderer_t& renderer = getRenderer();

    //create scene framebuffer
    colorRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R8G8B8A8_UNORM, true);
    normalDepthRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    render_target_handle_t targets[] = { colorRT_, normalDepthRT_ };
    sceneFBO_ = renderer.frameBufferCreate(targets, 2u);

    //create meshes
    mesh_handle_t teapot = renderer.meshCreate("../resources/teapot.obj", mesh::EXPORT_NORMALS_UVS);
    mesh_handle_t buddha = renderer.meshCreate("../resources/buddha.obj", mesh::EXPORT_NORMALS_UVS);
    mesh_handle_t plane = renderer.meshAdd(mesh::unitQuad(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer.shaderCreate("../ambient-occlusion/simple.shader");

    material_handle_t teapotMaterial = renderer.materialCreate(shader);
    renderer.getMaterial(teapotMaterial)->setProperty("globals.albedo", vec4(1.0f, 0.1f, 0.1f, 1.0f));

    material_handle_t buddhaMaterial = renderer.materialCreate(shader);
    renderer.getMaterial(buddhaMaterial)->setProperty("globals.albedo", vec4(0.1f, 1.0f, 0.1f, 1.0f));

    material_handle_t planeMaterial = renderer.materialCreate(shader);
    renderer.getMaterial(planeMaterial)->setProperty("globals.albedo", vec4(1.0f, 1.0f, 1.0f, 1.0f));

    //create actors
    mat4 teapotTransform = createTransform(vec3(-5.0f, -1.0f, 0.0f), VEC3_ONE, quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), degreeToRadian(30.0f)));
    renderer.actorCreate("teapot", teapot, teapotMaterial, teapotTransform);

    mat4 buddhaTransform = createTransform(vec3(5.0f, 3.0f, 0.0f), vec3(4.0f, 4.0f, 4.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-30.0f)));
    renderer.actorCreate("buddha", buddha, buddhaMaterial, buddhaTransform);

    mat4 planeTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(20.0f, 20.0f, 20.0f), quaternionFromAxisAngle(vec3(1, 0, 0), degreeToRadian(90.0f)));
    renderer.actorCreate("plane", plane, planeMaterial, planeTransform);

    //create camera
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera, &renderer);

    generateSSAOResources();
  }

  void generateSSAOResources()
  {
    //Generate random points in the normal-oriented hemishpere (in tangent space)
    std::vector<vec4> samples(ssaoSampleCount_);
    for (uint32_t i = 0; i < ssaoSampleCount_; i++)
    {
      vec3 sample = normalize(vec3(random(-1.0f, 1.0f),
        random(-1.0f, 1.0f),
        random(0.0f, 1.0f)));

      sample *= random(0.0f, 1.0f);
      samples[i] = vec4(sample, 1.0f);
    }

    render::context_t& context = getRenderContext();
    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      samples.data(), sizeof(vec4)*ssaoSampleCount_, nullptr,
      &ssaoKernelBuffer_);

    //Create a texture with random rotation vectors that will be tiled across the screen
    //to add some noise to the result
    image::image2D_t image = {};
    image.width = image.height = 4u;
    image.componentCount = 4u;
    image.componentSize = 4u;
    image.dataSize = image.width * image.height * image.componentCount * image.componentSize;
    image.data = new uint8_t[image.dataSize];
    vec4* data = (vec4*)image.data;
    for (uint32_t i = 0; i < 16; ++i)
      data[i] = vec4(random(-1.0f, 1.0f), random(-1.0f, 1.0f), 0.0f, 0.0f);

    render::texture2DCreate(context, &image, 1u, render::texture_sampler_t(), &ssaoNoise_);
    image::free(&image);

    //Create a framebuffer for ambient occlusion
    renderer_t& renderer = getRenderer();
    ssaoRT_ = renderer.renderTargetCreate(getWindowSize().x, getWindowSize().y, VK_FORMAT_R16_SFLOAT, false);
    ssaoFBO_ = renderer.frameBufferCreate(&ssaoRT_, 1u);

    //Create and configure ssao material 
    shader_handle_t ssaoShader = renderer.shaderCreate("../ambient-occlusion/ssao.shader");
    ssaoMaterial_ = renderer.materialCreate(ssaoShader);
    material_t* ssaoMaterialPtr = renderer.getMaterial(ssaoMaterial_);
    ssaoMaterialPtr->setProperty("globals.sampleCount", &ssaoSampleCount_);
    ssaoMaterialPtr->setBuffer("ssaoKernel", ssaoKernelBuffer_);
    ssaoMaterialPtr->setTexture("normalDepthTexture", normalDepthRT_);
    ssaoMaterialPtr->setTexture("ssaoNoise", ssaoNoise_);    

    //Create and configure blur material
    shader_handle_t blurShader = renderer.shaderCreate("../ambient-occlusion/blur.shader");
    blurMaterial_ = renderer.materialCreate(blurShader);
    renderer.getMaterial(blurMaterial_)->setTexture("sceneColorTexture", colorRT_);
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    cameraController_.onKey(key, pressed);
  }

  void onMouseMove(const vec2& mousePos, const vec2 &mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
      cameraController_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
  }

  void onQuit()
  {
    render::gpuBufferDestroy(getRenderContext(), nullptr, &ssaoKernelBuffer_);
    render::textureDestroy(getRenderContext(), &ssaoNoise_);
  }

  void render()
  {
    beginFrame();

    renderer_t& renderer = getRenderer();

    camera_handle_t camera = cameraController_.getCameraHandle();
    renderer.setupCamera(camera);

    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera, &visibleActors);

    //Render scene
    command_buffer_t renderSceneCmd(&renderer, "Render");
    renderSceneCmd.setFrameBuffer(sceneFBO_);
    renderSceneCmd.changeLayout(colorRT_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    renderSceneCmd.changeLayout(normalDepthRT_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submitAndRelease();

    if (ssaoEnabled_)
    {
      material_t* ssaoMaterialPtr = renderer.getMaterial(ssaoMaterial_);
      ssaoMaterialPtr->setProperty("globals.radius", &ssaoRadius_);
      ssaoMaterialPtr->setProperty("globals.bias", &ssaoBias_);      

      command_buffer_t ssaoPass = command_buffer_t(&renderer, "SSAO");
      ssaoPass.setFrameBuffer(ssaoFBO_);
      ssaoPass.changeLayout(normalDepthRT_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      ssaoPass.changeLayout(ssaoRT_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      ssaoPass.blit(BKK_NULL_HANDLE, ssaoMaterial_);
      ssaoPass.submitAndRelease();

      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer, "Blur", renderer.getRenderCompleteSemaphore());
      blitToBackbufferCmd.changeLayout(colorRT_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      blitToBackbufferCmd.changeLayout(ssaoRT_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      blitToBackbufferCmd.blit(ssaoRT_, blurMaterial_);
      blitToBackbufferCmd.submitAndRelease();
    }
    else
    {
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer, "Blit", renderer.getRenderCompleteSemaphore());
      blitToBackbufferCmd.changeLayout(colorRT_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      blitToBackbufferCmd.blit(colorRT_);
      blitToBackbufferCmd.submitAndRelease();
    }

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");

    ImGui::LabelText("", "SSAO Settings");
    ImGui::Checkbox("Enable", &ssaoEnabled_);
    ImGui::SliderFloat("Radius", &ssaoRadius_, 0.0f, 10.0f);
    ImGui::SliderFloat("Bias", &ssaoBias_, 0.0f, 1.0f);

    ImGui::End();
  }

private:

  frame_buffer_handle_t sceneFBO_;
  render_target_handle_t colorRT_;
  render_target_handle_t normalDepthRT_;

  free_camera_controller_t cameraController_;

  //SSAO
  bool ssaoEnabled_;
  uint32_t ssaoSampleCount_;
  float ssaoRadius_;
  float ssaoBias_;
  frame_buffer_handle_t ssaoFBO_;
  render_target_handle_t ssaoRT_;
  material_handle_t ssaoMaterial_;
  render::gpu_buffer_t ssaoKernelBuffer_;
  render::texture_t ssaoNoise_;
  material_handle_t blurMaterial_;
};

int main()
{
  ambient_occlusion_sample_t().run();
  return 0;
}

