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
#include "core/string-utils.h"

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::core::maths;
using namespace bkk::framework;

class multithreading_sample_t : public application_t
{
public:
  multithreading_sample_t(const uvec2& imageSize)
    :application_t("Multithreading sample", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(-1.1f, 0.1f, -0.1f), vec2(0.2f, 1.57f), 0.03f, 0.01f),
    globals_()
  {
    //Create global buffer
    globals_.lightDirection_ = vec4(1.0f, 1.0f, 0.0f, 0.0f);
    globals_.fogPlane_ = vec4(0.0f, 1.0f, 0.0f, 0.0f);
    globals_.fogProperties_ = vec4(1.0f, 1.0f, 1.0f, 2.5f);

    render::gpuBufferCreate(getRenderer().getContext(), render::gpu_buffer_t::UNIFORM_BUFFER,
      &globals_, sizeof(globals_), nullptr, &globalsBuffer_);
    
    loadScene("../resources/sponza/sponza.obj");

    //Allocate command buffers
    renderer_t& renderer = getRenderer(); 
    commandBuffers_.resize(renderer.getThreadPool()->getThreadCount());

    //create camera
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.01f, 500.0f));
    cameraController_.setCameraHandle(camera, &renderer);
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
    for (u32 i(0); i < materialCount; ++i)
    {
      materialHandles[i] = renderer.materialCreate(shader);
      material_t* materialPtr = renderer.getMaterial(materialHandles[i]);
      materialPtr->setProperty("properties.kd", vec4(materials[i].kd, 1.0f) );
      materialPtr->setProperty("properties.ks", vec4(materials[i].ks, 1.0f));
      materialPtr->setBuffer("globals", globalsBuffer_);

      if (strlen(materials[i].diffuseMap) > 0)
      {
        std::string diffuseMapPath = path.substr(0, path.find_last_of('/')+1 );
        diffuseMapPath += materials[i].diffuseMap;

        image::image2D_t image = {};
        if (image::load(diffuseMapPath.c_str(), true, &image))
        {
          //Create the texture
          render::texture_t texture;
          render::texture2DCreateAndGenerateMipmaps(renderer.getContext(), image, render::texture_sampler_t(), &texture);
          image::free(&image);
          textures_.push_back(texture);
          materialPtr->setTexture("MainTexture", texture);
        }
      }
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
    mat4 projectionMatrix = perspectiveProjectionMatrix(1.2f, (f32)width / (f32)height, 0.1f, 100.0f);
    cameraController_.getCamera()->setProjectionMatrix(projectionMatrix);
  }

  void onQuit()
  {
    for (uint32_t i(0); i < textures_.size(); ++i)
      render::textureDestroy(getRenderer().getContext(), &textures_[i]);
  }

  void render()
  {
    beginFrame();

    renderer_t& renderer = getRenderer();
    camera_handle_t camera = cameraController_.getCameraHandle();
    renderer.setupCamera(camera);

    actor_t* visibleActors = nullptr;
    uint32_t actorCount = renderer.getVisibleActors(camera, &visibleActors);

    generateCommandBuffersParallel(&renderer, "parallelCommandBuffer",
                                   BKK_NULL_HANDLE, &VEC4_ZERO,
                                   visibleActors, actorCount, "OpaquePass",
                                   renderer.getRenderCompleteSemaphore(),
                                   &commandBuffers_[0], (uint32_t)commandBuffers_.size());

    for (uint32_t i(0); i < commandBuffers_.size(); ++i)
      commandBuffers_[i].submitAndRelease();

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");

    ImGui::LabelText("", "Fog");
    ImGui::SliderFloat3("Light direction", &globals_.lightDirection_.x, -1.0f, 1.0f);
    ImGui::SliderFloat3("Fog Plane Normal", &globals_.fogPlane_.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Fog Plane Offset", &globals_.fogPlane_.a, -1.0f, 1.0f);
    ImGui::ColorEdit3("Fog Color", &globals_.fogProperties_.x);
    ImGui::SliderFloat("Fog Density", &globals_.fogProperties_.a, 0.0f, 10.0f);
    ImGui::End();
    
    render::gpuBufferUpdate(getRenderer().getContext(), &globals_, 0u, sizeof(globals_), &globalsBuffer_);
  }

private:
  free_camera_controller_t cameraController_;
  std::vector<render::texture_t> textures_;
  std::vector<command_buffer_t> commandBuffers_;
  render::gpu_buffer_t globalsBuffer_;
  
  struct
  {
    vec4 lightDirection_;
    vec4 fogPlane_;
    vec4 fogProperties_;
  }globals_;

  
  uint32_t actorCount_;
  uint32_t vertexCount_;
  uint32_t triangleCount_;
};

int main()
{  
  multithreading_sample_t(uvec2(1200u, 800u)).run();
  return 0;
}

