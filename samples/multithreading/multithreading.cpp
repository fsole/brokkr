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

class multithreading_sample_t : public application_t
{
public:
  multithreading_sample_t(const uvec2& imageSize, const uint32_t shadowMapSize)
    :application_t("Multithreading sample", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(-1.1f, 0.1f, -0.1f), vec2(0.2f, 1.57f), 0.03f, 0.01f),
    sceneCommandBuffers_(getRenderer().getThreadPool()->getThreadCount()),
    shadowCommandBuffers_(getRenderer().getThreadPool()->getThreadCount())
  {
    renderer_t& renderer = getRenderer();
    
    //create camera and camera controller
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.01f, 100.0f));
    cameraController_.setCameraHandle(camera, &renderer);

    //Create camera used to render the shadow map
    camera_t shadowCamera = camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, 1.0f, 0.1f, 5.0f);
    shadowCamera.setViewToWorldMatrix(maths::createTransform(vec3(0.0f, 2.0f, 0.0f), VEC3_ONE, QUAT_UNIT));
    shadowCamera_ = renderer.cameraAdd(shadowCamera);

    //Create shadow map and fbo
    shadowMap_ = renderer.renderTargetCreate(shadowMapSize, shadowMapSize, VK_FORMAT_R16_SFLOAT, true);
    shadowFBO_ = renderer.frameBufferCreate(&shadowMap_, 1u);

    //Globals buffer
    globals_.light_ = vec4(0.0f, 1.0f, 0.0f, 1.0f);
    globals_.fogPlane_ = vec4(0.0f, -1.0f, 0.0f, 0.0f);
    globals_.fogProperties_ = vec4(1.0f, 1.0f, 1.0f, 0.5f);
    globals_.worldToLightClipSpace_ = shadowCamera.getViewProjectionMatrix();
    globals_.shadowMapSize_ = shadowMapSize;

    render::gpuBufferCreate(renderer.getContext(), render::gpu_buffer_t::UNIFORM_BUFFER,
      &globals_, sizeof(globals_), nullptr, &globalsBuffer_);

    loadScene("../resources/sponza/sponza.obj");
  }

  bool loadTexture(const std::string& file, render::texture_t* texture)
  {
    image::image2D_t image = {};
    if (image::load(file.c_str(), true, &image))
    {
      render::texture2DCreateAndGenerateMipmaps(getRenderer().getContext(), image, render::texture_sampler_t(), texture);
      image::free(&image);
      textures_.push_back(*texture);
      return true;
    }

    return false;
  }

  void loadScene(const std::string& path)
  {
    renderer_t& renderer = getRenderer();

    //Load materials
    shader_handle_t shader = renderer.shaderCreate("../multithreading/diffuse.shader");
    mesh::material_data_t* materials;
    uint32_t* materialIndex;
    uint32_t materialCount = mesh::loadMaterialData(path.c_str(), &materialIndex, &materials);
    std::vector<material_handle_t> materialHandles(materialCount);
    std::string basePath = path.substr(0, path.find_last_of('/') + 1);
    for (u32 i(0); i < materialCount; ++i)
    {
      materialHandles[i] = renderer.materialCreate(shader);
      material_t* materialPtr = renderer.getMaterial(materialHandles[i]);
      materialPtr->setBuffer("globals", globalsBuffer_);
      materialPtr->setProperty("properties.kd", vec4(materials[i].kd, 1.0f) );
      materialPtr->setProperty("properties.ks", vec4(materials[i].ks, 1.0f));
      materialPtr->setProperty("properties.shininess", materials[i].shininess);
      
      //Textures
      materialPtr->setTexture("shadowMap", shadowMap_);

      render::texture_t texture;
      if (loadTexture(basePath + materials[i].diffuseMap, &texture))
        materialPtr->setTexture("diffuseTexture", texture);

      materialPtr->setTexture("normalTexture", renderer.getDefaultNormalTexture());
      if (loadTexture(basePath + materials[i].normalMap, &texture))
        materialPtr->setTexture("normalTexture", texture);      
      
      if (loadTexture(basePath + materials[i].specularMap, &texture))
        materialPtr->setTexture("specularTexture", texture);
      
      if (loadTexture(basePath + materials[i].opacityMap, &texture))
        materialPtr->setTexture("opacityTexture", texture);      
    }
    delete[] materials;

    //Load meshes and create actors
    mat4 transform = createTransform(vec3(0.0f, -0.5f, 0.0f), vec3(0.001f), QUAT_UNIT);
    mesh::mesh_t* mesh = nullptr;
    uint32_t meshCount = mesh::createFromFile(renderer.getContext(), path.c_str(), mesh::EXPORT_ALL, nullptr, &mesh);
    for (u32 i(0); i < meshCount; ++i)
    {
      mesh_handle_t meshHandle = renderer.meshAdd(mesh[i]);
      renderer.actorCreate("actor", meshHandle, materialHandles[materialIndex[i]], transform);
    }
    delete[] mesh;
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

  void onResize(uint32_t width, uint32_t height)
  {
    mat4 projectionMatrix = perspectiveProjectionMatrix(1.2f, (f32)width / (f32)height, 0.01f, 100.0f);
    cameraController_.getCamera()->setProjectionMatrix(projectionMatrix);
  }

  void onQuit()
  {
    render::context_t& context = getRenderer().getContext();
    render::gpuBufferDestroy(context, nullptr, &globalsBuffer_);

    for (uint32_t i(0); i < textures_.size(); ++i)
      render::textureDestroy(context, &textures_[i]);
  }

  void render()
  {
    beginFrame();

    renderer_t& renderer = getRenderer();


    //Render shadow map
    vec3 lightDirection = normalize(globals_.light_.xyz());
    maths::mat4 viewToWorldMatrix = maths::createTransform(maths::vec3(0.0f, 0.0f, 2.0f), maths::VEC3_ONE, maths::QUAT_UNIT) * 
                                    maths::createTransform(maths::VEC3_ZERO, maths::VEC3_ONE, maths::quat(VEC3_FORWARD, lightDirection));

    renderer.getCamera(shadowCamera_)->setViewToWorldMatrix(viewToWorldMatrix);
    renderer.setupCamera(shadowCamera_);

    actor_t* visibleActors = nullptr;
    uint32_t actorCount = renderer.getVisibleActors(shadowCamera_, &visibleActors);

    generateCommandBuffersParallel(&renderer, "parallelCommandBuffer",
      shadowFBO_, &VEC4_ZERO,
      visibleActors, actorCount, "DepthPass",
      VK_NULL_HANDLE,
      &shadowCommandBuffers_[0], (uint32_t)shadowCommandBuffers_.size());

    for (uint32_t i(0); i < shadowCommandBuffers_.size(); ++i)
      shadowCommandBuffers_[i].submitAndRelease();

    //Render scene
    camera_handle_t camera = cameraController_.getCameraHandle();
    renderer.setupCamera(camera);

    visibleActors = nullptr;
    actorCount = renderer.getVisibleActors(camera, &visibleActors);

    generateCommandBuffersParallel(&renderer, "parallelCommandBuffer",
      BKK_NULL_HANDLE, &VEC4_ONE,
      visibleActors, actorCount, "OpaquePass",
      renderer.getRenderCompleteSemaphore(),
      &sceneCommandBuffers_[0], (uint32_t)sceneCommandBuffers_.size());

    for (uint32_t i(0); i < sceneCommandBuffers_.size(); ++i)
      sceneCommandBuffers_[i].submitAndRelease();
    
    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");    
    ImGui::SliderFloat3("Light direction", &globals_.light_.x, -1.0f, 1.0f); 
    ImGui::SliderFloat("Light Intensity", &globals_.light_.w, 0.0f, 1.0f);
    ImGui::SliderFloat3("Fog Plane Normal", &globals_.fogPlane_.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Fog Plane Offset", &globals_.fogPlane_.a, -1.0f, 1.0f);
    ImGui::ColorEdit3("Fog Color", &globals_.fogProperties_.x);
    ImGui::SliderFloat("Fog Density", &globals_.fogProperties_.a, 0.0f, 10.0f);
    ImGui::End();
    
    globals_.worldToLightClipSpace_ = getRenderer().getCamera(shadowCamera_)->getViewProjectionMatrix();
    render::gpuBufferUpdate(getRenderer().getContext(), &globals_, 0u, sizeof(globals_), &globalsBuffer_);
  }

private:
  free_camera_controller_t cameraController_;
  
  render::gpu_buffer_t globalsBuffer_;
  std::vector<render::texture_t> textures_;

  std::vector<command_buffer_t> sceneCommandBuffers_;
  std::vector<command_buffer_t> shadowCommandBuffers_;
  camera_handle_t shadowCamera_;
  render_target_handle_t shadowMap_;
  frame_buffer_handle_t shadowFBO_;

  struct
  {    
    vec4 light_;                         //xyz is direction, w is intensity
    vec4 fogPlane_;                      //xyz is normal, w is offset
    vec4 fogProperties_;                 //rgb is color, a is density
    maths::mat4 worldToLightClipSpace_;  //Transforms points from world space to light clip space
    uint32_t shadowMapSize_;             //Shadow map resolution
  }globals_;
};

int main()
{  
  multithreading_sample_t(uvec2(1200u, 800u), 4096u).run();
  return 0;
}

