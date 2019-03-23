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

class framework_test_t : public application_t
{
public:
  framework_test_t()
    :application_t("Framework test", 1200u, 800u, 3u),
    cameraController_(vec3(0.0f, 4.0f, 12.0f), vec2(0.1f, 0.0f), 1.0f, 0.01f),
    ssaoEnabled_(true),
    ssaoRadius_(0.5f),
    ssaoBias_(0.025f),
    lightIntensity_(1.0f),
    exposure_(1.5f)
  {
    uvec2 imageSize(1200u, 800u);
    renderer_t& renderer = getRenderer();

    //create scene framebuffer
    colorRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    normalsRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    render_target_handle_t targets[] = { colorRT_, normalsRT_ };
    sceneFBO_ = renderer.frameBufferCreate(targets, 2u);

    //create meshes
    mesh::mesh_t teapotMesh;
    mesh::createFromFile(getRenderContext(), "../resources/teapot.obj", mesh::EXPORT_ALL, nullptr, 0, &teapotMesh);
    mesh_handle_t teapot = renderer.addMesh(teapotMesh);
    mesh::mesh_t buddhaMesh;
    mesh::createFromFile(getRenderContext(), "../resources/buddha.obj", mesh::EXPORT_ALL, nullptr, 0, &buddhaMesh);
    mesh_handle_t buddha = renderer.addMesh(buddhaMesh);

    mesh_handle_t plane = renderer.addMesh(mesh::unitQuad(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer.shaderCreate("../ambient-occlusion/simple.shader");
    material_handle_t material0 = renderer.materialCreate(shader);
    material_t* materialPtr = renderer.getMaterial(material0);
    materialPtr->setProperty("globals.albedo", vec4(1.0f, 0.1f, 0.1f, 1.0f));

    material_handle_t material1 = renderer.materialCreate(shader);
    materialPtr = renderer.getMaterial(material1);
    materialPtr->setProperty("globals.albedo", vec4(0.1f, 1.0f, 0.1f, 1.0f));

    material_handle_t material2 = renderer.materialCreate(shader);
    materialPtr = renderer.getMaterial(material2);
    materialPtr->setProperty("globals.albedo", vec4(1.0f, 1.0f, 1.0f, 1.0f));

    //create actors
    mat4 transform = createTransform(vec3(-5.0f, -1.0f, 0.0f), VEC3_ONE, quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), degreeToRadian(30.0f)));
    renderer.actorCreate("teapot", teapot, material0, transform);

    transform = createTransform(vec3(5.0f, 3.0f, 0.0f), vec3(4.0f,4.0f,4.0f), maths::quaternionFromAxisAngle(vec3(1, 0, 0), maths::degreeToRadian(90.0f))*maths::quaternionFromAxisAngle(vec3(0, 1, 0), maths::degreeToRadian(-30.0f)));
    renderer.actorCreate("buddha", buddha, material1, transform);

    transform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(20.0f, 20.0f, 20.0f), quaternionFromAxisAngle(vec3(1, 0, 0), degreeToRadian(90.0f)));
    renderer.actorCreate("plane", plane, material2, transform);
  
    generateSSAOResources();

    //create camera
    camera_ = renderer.addCamera(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera_, &renderer);
  }

  void generateSSAOResources()
  {
    std::vector<vec4> samples(ssaoSampleCount);
    for (uint32_t i = 0; i < ssaoSampleCount; i++)
    {
      vec3 sample = normalize( vec3(random(-1.0f, 1.0f),
                              random(-1.0f, 1.0f),
                              random(0.0f, 1.0f)));

      sample *= random(0.0f, 1.0f);
      samples[i] = vec4(sample, 1.0f);
    }

    render::context_t& context = getRenderContext();
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage_e::STORAGE_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      samples.data(), sizeof(vec4)*ssaoSampleCount, nullptr,
      &ssaoKernelBuffer_ );
    
    image::image2D_t image = {};
    image.width = image.height = 4u;
    image.componentCount = 4u;
    image.componentSize = 4u;
    image.dataSize = image.width * image.height * image.componentCount * image.componentSize;
    image.data = new uint8_t[image.dataSize];
    vec4* data = (vec4*)image.data;
    for (uint32_t i = 0; i < 16; ++i)
    {
      data[i] = vec4( random(-1.0f, 1.0f), random(-1.0f, 1.0f), 0.0f, 0.0f );
    }

    render::texture2DCreate(context, &image, 1u, render::texture_sampler_t(), &ssaoNoise_);
    delete[] image.data;

    renderer_t& renderer = getRenderer();
    shader_handle_t shader = renderer.shaderCreate("../ambient-occlusion/ssao.shader");
    ssaoMaterial_ = renderer.materialCreate(shader);
    material_t* ssaoMaterial = renderer.getMaterial(ssaoMaterial_);
    ssaoMaterial->setBuffer("ssaoKernel", ssaoKernelBuffer_);
    ssaoMaterial->setTexture("GBuffer0", colorRT_);
    ssaoMaterial->setTexture("GBuffer1", normalsRT_);
    ssaoMaterial->setTexture("ssaoNoise", ssaoNoise_ );
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
    {
      float delta = 0.5f;
      switch (key)
      {
      case window::key_e::KEY_UP:
      case 'w':
        cameraController_.Move(0.0f, -delta);
        break;

      case window::key_e::KEY_DOWN:
      case 's':
        cameraController_.Move(0.0f, delta);
        break;

      case window::key_e::KEY_LEFT:
      case 'a':
        cameraController_.Move(-delta, 0.0f);
        break;

      case window::key_e::KEY_RIGHT:
      case 'd':
        cameraController_.Move(delta, 0.0f);
        break;

      default:
        break;
      }
    }
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

    //Render scene
    renderer.setupCamera(camera_);
    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera_, &visibleActors);

    command_buffer_t renderSceneCmd(&renderer, sceneFBO_);
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submit();
    renderSceneCmd.release();

    if (ssaoEnabled_)
    {
      material_t* ssaoMaterialPtr = renderer.getMaterial(ssaoMaterial_);
      ssaoMaterialPtr->setProperty("globals.radius", ssaoRadius_);
      ssaoMaterialPtr->setProperty("globals.bias", ssaoBias_);

      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer);
      blitToBackbufferCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(NULL_HANDLE, ssaoMaterial_);
      blitToBackbufferCmd.submit();
      blitToBackbufferCmd.release();
    }
    else
    {
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer);
      blitToBackbufferCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(colorRT_);
      blitToBackbufferCmd.submit();
      blitToBackbufferCmd.release();
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
  struct light_t
  {
    vec4 position;
    vec3 color;
    float radius;
  };

  frame_buffer_handle_t sceneFBO_;
  render_target_handle_t colorRT_;
  render_target_handle_t normalsRT_;
  
  camera_handle_t camera_;
  free_camera_t cameraController_;

  //SSAO
  bool ssaoEnabled_;
  float ssaoRadius_;
  float ssaoBias_;
  material_handle_t ssaoMaterial_;
  int ssaoSampleCount = 64;
  render::gpu_buffer_t ssaoKernelBuffer_;
  render::texture_t ssaoNoise_;

  float lightIntensity_;
  float exposure_;
};

int main()
{
  framework_test_t test;
  test.loop();

  return 0;
}