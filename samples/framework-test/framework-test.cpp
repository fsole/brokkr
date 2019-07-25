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
using namespace bkk::framework;

class framework_test_t : public application_t
{
public:
  framework_test_t()
  :application_t("Framework test", 1200u, 800u, 3u),
   cameraController_(maths::vec3(0.0f, 4.0f, 12.0f), maths::vec2(0.1f, 0.0f), 0.5f, 0.01f),
   bloomEnabled_(true),
   bloomTreshold_(1.0f),
   lightIntensity_(1.0f),
   exposure_(1.5f)
  {
    maths::uvec2 imageSize(1200u, 800u);
    renderer_t& renderer = getRenderer();

    //create scene framebuffer
    sceneRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    sceneFBO_ = renderer.frameBufferCreate(&sceneRT_, 1u);

    //create light buffer
    lightBuffer_ = createLightBuffer();

    //Load environment map
    image::image2D_t cubemapImage = {};
    image::load("../resources/Circus_Backstage_3k.hdr", true, &cubemapImage);
    render::textureCubemapCreateFromEquirectangularImage(getRenderContext(), cubemapImage, 2046u, true, &skybox_);
    render::diffuseConvolution(getRenderContext(), skybox_, 64u, &irradianceMap_);
    render::specularConvolution(getRenderContext(), skybox_, 256u, 4u, &specularMap_);
    render::brdfConvolution(getRenderContext(), 512u, &brdfLut_);
    image::free(&cubemapImage);

    shader_handle_t skyboxShader = renderer.shaderCreate("../../shaders/sky-box.shader");
    skyboxMaterial_ = renderer.materialCreate(skyboxShader);
    renderer.getMaterial(skyboxMaterial_)->setTexture("CubeMap", skybox_);

    //create meshes
    mesh_handle_t teapot = renderer.meshCreate("../resources/teapot.obj", mesh::EXPORT_ALL);
    mesh_handle_t plane = renderer.meshAdd(mesh::unitQuad(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer.shaderCreate("../framework-test/pbr.shader");
    material_handle_t material0 = renderer.materialCreate(shader);
    material_t* materialPtr = renderer.getMaterial(material0);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.1f, 0.1f, 0.1f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.9f, 0.9f, 0.9f));
    materialPtr->setProperty("globals.roughness", 0.15f);
    materialPtr->setProperty("globals.metallic", 0.8f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    material_handle_t material1 = renderer.materialCreate(shader);
    materialPtr = renderer.getMaterial(material1);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.5f, 0.5f, 0.5f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.6f, 0.6f, 0.6f));
    materialPtr->setProperty("globals.roughness", 0.3f);
    materialPtr->setProperty("globals.metallic", 0.3f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    material_handle_t material2 = renderer.materialCreate(shader);
    materialPtr = renderer.getMaterial(material2);
    materialPtr->setProperty("globals.albedo", maths::vec3(0.1f, 0.1f, 0.1f));
    materialPtr->setProperty("globals.F0", maths::vec3(0.0f, 0.0f, 0.0f));
    materialPtr->setProperty("globals.roughness", 1.0f);
    materialPtr->setProperty("globals.metallic", 0.0f);
    materialPtr->setTexture("irradianceMap", irradianceMap_);
    materialPtr->setTexture("specularMap", specularMap_);
    materialPtr->setTexture("brdfLUT", brdfLut_);
    materialPtr->setBuffer("lights", lightBuffer_);

    //create actors
    maths::mat4 transform = maths::createTransform(maths::vec3(-5.0f, -1.0f, 0.0f), maths::VEC3_ONE, maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), maths::degreeToRadian(30.0f)));
    renderer.actorCreate("teapot0", teapot, material0, transform);

    transform = maths::createTransform(maths::vec3(5.0f, -1.0f, 0.0f), maths::VEC3_ONE, maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), maths::degreeToRadian(150.0f)));
    renderer.actorCreate("teapot1", teapot, material1, transform);
    
    transform = maths::createTransform(maths::vec3(0.0f, -1.0f, 0.0f), maths::vec3(20.0f, 20.0f, 20.0f), maths::quaternionFromAxisAngle(maths::vec3(1, 0, 0), maths::degreeToRadian(90.0f)) );
    renderer.actorCreate("plane", plane, material2, transform);
    
    //Bloom resources
    brightPixelsRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    brightPixelsFBO_ = renderer.frameBufferCreate(&brightPixelsRT_, 1u);
    blurVerticalRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    blurVerticalFBO_ = renderer.frameBufferCreate(&blurVerticalRT_, 1u);
    bloomRT_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    bloomFBO_ = renderer.frameBufferCreate(&bloomRT_, 1u);
    shader_handle_t bloomShader = renderer.shaderCreate("../framework-test/bloom.shader");
    bloomMaterial_ = renderer.materialCreate(bloomShader);
    shader_handle_t blendShader = renderer.shaderCreate("../framework-test/blend.shader");
    blendMaterial_ = renderer.materialCreate(blendShader);
    material_t* blendMaterial = renderer.getMaterial(blendMaterial_);
    blendMaterial->setTexture("bloomBlur", renderer.getRenderTarget(bloomRT_)->getColorBuffer());

    //create camera
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x/(float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera, &renderer);
  }
  
  render::gpu_buffer_t createLightBuffer()
  {    
    int lightCount = 2;
    std::vector<light_t> lights(lightCount);
    lights[0].position = maths::vec4(-7.0f, 5.0f, 0.0f, 1.0f);
    lights[0].color = maths::vec3(1.0f, 1.0f, 1.0f);
    lights[0].radius = 13.0f;
    lights[1].position = maths::vec4(7.0f, 5.0f, 0.0f, 1.0f);
    lights[1].color = maths::vec3(1.0f, 1.0f, 1.0f);
    lights[1].radius = 13.0f;

    //Create buffer
    render::gpu_buffer_t lightBuffer = {};
    render::context_t& context = getRenderContext();
    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      nullptr, sizeof(light_t)*lightCount + sizeof(maths::vec4), nullptr,
      &lightBuffer );
    
    render::gpuBufferUpdate(context, &lightCount, 0u, sizeof(int), &lightBuffer);
    render::gpuBufferUpdate(context, &lightIntensity_, sizeof(int), sizeof(float), &lightBuffer);
    render::gpuBufferUpdate(context, lights.data(), sizeof(maths::vec4), lightCount * sizeof(light_t), &lightBuffer);

    return lightBuffer;
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    cameraController_.onKey(key, pressed);
  }

  void onMouseMove(const maths::vec2& mousePos, const maths::vec2 &mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
      cameraController_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);    
  }

  void onResize(uint32_t width, uint32_t height)
  {
    maths::mat4 projectionMatrix = maths::perspectiveProjectionMatrix(1.2f, (f32)width / (f32)height, 0.1f, 100.0f);
    cameraController_.getCamera()->setProjectionMatrix(projectionMatrix);
  }

  void onQuit() 
  {
    render::gpuBufferDestroy(getRenderContext(), nullptr, &lightBuffer_);
    render::textureDestroy(getRenderContext(), &skybox_);
    render::textureDestroy(getRenderContext(), &irradianceMap_);
    render::textureDestroy(getRenderContext(), &specularMap_);
    render::textureDestroy(getRenderContext(), &brdfLut_);
  }

  void render()
  {
    beginFrame();

    renderer_t& renderer = getRenderer();

    //Update global properties
    renderer.getMaterial(blendMaterial_)->setProperty("globals.exposure", exposure_);
    render::gpuBufferUpdate(getRenderContext(), &lightIntensity_, sizeof(int), sizeof(float), &lightBuffer_);
    
    //Render scene
    camera_handle_t camera = cameraController_.getCameraHandle();
    renderer.setupCamera(camera);
    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera, &visibleActors);

    command_buffer_t renderSceneCmd(&renderer);
    renderSceneCmd.setFrameBuffer(sceneFBO_);
    renderSceneCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submitAndRelease();
    
    //Render skybox
    command_buffer_t renderSkyboxCmd = command_buffer_t(&renderer);
    renderSkyboxCmd.setFrameBuffer(sceneFBO_);
    renderSkyboxCmd.setDependencies(&renderSceneCmd, 1u);
    renderSkyboxCmd.blit(bkk::core::BKK_NULL_HANDLE, skyboxMaterial_ );
    renderSkyboxCmd.submitAndRelease();
    
    if (bloomEnabled_)
    {
      material_t* bloomMaterial = renderer.getMaterial(bloomMaterial_);
      bloomMaterial->setProperty("globals.bloomTreshold", bloomTreshold_);
      
      //Extract bright pixels from scene render target
      command_buffer_t extractBrightPixelsCmd = command_buffer_t(&renderer);
      extractBrightPixelsCmd.setFrameBuffer(brightPixelsFBO_);
      extractBrightPixelsCmd.setDependencies(&renderSkyboxCmd, 1u);
      extractBrightPixelsCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      extractBrightPixelsCmd.blit(sceneRT_ , bloomMaterial_, "extractBrightPixels" );
      extractBrightPixelsCmd.submitAndRelease();
      
      //Blur vertical pass
      command_buffer_t blurVerticalCmd = command_buffer_t(&renderer);
      blurVerticalCmd.setFrameBuffer(blurVerticalFBO_);
      blurVerticalCmd.setDependencies(&extractBrightPixelsCmd, 1u);
      blurVerticalCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blurVerticalCmd.blit(brightPixelsRT_, bloomMaterial_, "blurVertical");
      blurVerticalCmd.submitAndRelease();

      //Blur horizontal pass
      command_buffer_t blurHorizontalCmd = command_buffer_t(&renderer);
      blurHorizontalCmd.setFrameBuffer(bloomFBO_);
      blurHorizontalCmd.setDependencies(&blurVerticalCmd, 1u);
      blurHorizontalCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blurHorizontalCmd.blit(blurVerticalRT_, bloomMaterial_, "blurHorizontal");
      blurHorizontalCmd.submitAndRelease();

      //Blend bloom and scene render targets
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer);
      blitToBackbufferCmd.setDependencies(&blurHorizontalCmd, 1u);
      blitToBackbufferCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(sceneRT_, blendMaterial_, "blend" );
      blitToBackbufferCmd.submitAndRelease();
    }
    else
    {
      //Copy scene render target to the back buffer
      command_buffer_t blitToBackbufferCmd = command_buffer_t(&renderer);
      blitToBackbufferCmd.clearRenderTargets(maths::vec4(0.0f, 0.0f, 0.0f, 1.0f));
      blitToBackbufferCmd.blit(sceneRT_, blendMaterial_);
      blitToBackbufferCmd.submitAndRelease();
    }

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");

    ImGui::LabelText("", "General Settings");
    ImGui::SliderFloat("Light Intensity", &lightIntensity_, 0.0f, 10.0f);
    ImGui::SliderFloat("Exposure", &exposure_, 0.0f, 10.0f);

    ImGui::Separator();

    ImGui::LabelText("", "Bloom Settings");
    ImGui::Checkbox("Enable", &bloomEnabled_);
    ImGui::SliderFloat("Bloom Treshold", &bloomTreshold_, 0.0f, 10.0f);

    ImGui::End();
  }

private:
  struct light_t
  {
    maths::vec4 position;
    maths::vec3 color;
    float radius;
  };

  frame_buffer_handle_t sceneFBO_;
  render_target_handle_t sceneRT_;  
  render::gpu_buffer_t lightBuffer_;
  material_handle_t skyboxMaterial_;
  render::texture_t skybox_;
  render::texture_t irradianceMap_;
  render::texture_t specularMap_;
  render::texture_t brdfLut_;

  bool bloomEnabled_;
  material_handle_t bloomMaterial_;
  material_handle_t blendMaterial_;
  frame_buffer_handle_t bloomFBO_;
  render_target_handle_t bloomRT_;
  frame_buffer_handle_t blurVerticalFBO_;
  render_target_handle_t blurVerticalRT_;
  frame_buffer_handle_t brightPixelsRT_;
  render_target_handle_t brightPixelsFBO_;
  float bloomTreshold_;

  free_camera_controller_t cameraController_;

  float lightIntensity_;
  float exposure_;
};

int main()
{
  framework_test_t().run();
  return 0;
}