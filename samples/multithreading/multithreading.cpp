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
  multithreading_sample_t(const uvec2& imageSize)
    :application_t("Multithreading sample", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(-1.1f, 0.6f, -0.1f), vec2(0.2f, 1.57f), 0.03f, 0.01f)
  {
    renderer_t& renderer = getRenderer();

    shader_handle_t shader = renderer.shaderCreate("../multithreading/diffuse.shader");
    mesh::material_data_t* materials;
    uint32_t* materialIndex;
    uint32_t materialCount = mesh::loadMaterialData("../resources/sponza/sponza.obj", &materialIndex, &materials);
    std::vector<material_handle_t> materialHandles(materialCount);
    for (u32 i(0); i < materialCount; ++i)
    {
      materialHandles[i] = renderer.materialCreate(shader);
      material_t* materialPtr = renderer.getMaterial(materialHandles[i]);
      materialPtr->setProperty("globals.albedo", vec4(1.0f));
      materialPtr->setProperty("globals.lightDirection", vec4(normalize(vec3(1.0f, 0.0f, 1.0f)), 0.0f));

      if (strlen(materials[i].diffuseMap) > 0)
      {
        std::string diffuseMapPath = "../resources/sponza/";
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

    mat4 transform = createTransform(vec3(0.0f), vec3(0.001f), QUAT_UNIT);
    mesh::mesh_t* mesh = nullptr;
    uint32_t meshCount = mesh::createFromFile(renderer.getContext(), "../resources/sponza/sponza.obj", mesh::EXPORT_ALL, nullptr, &mesh);
    for (u32 i(0); i < meshCount; ++i)
    {
      mesh_handle_t meshHandle = renderer.meshAdd(mesh[i]);
      renderer.actorCreate("actor", meshHandle, materialHandles[materialIndex[i]], transform);
    }
    delete[] mesh;
    
    //create camera
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.01f, 500.0f));
    cameraController_.setCameraHandle(camera, &renderer);
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
    int count = renderer.getVisibleActors(camera, &visibleActors);

    if (count >= 10)
    {
      command_buffer_t* commandBuffers = nullptr;
      generateCommandBuffersParallel(&renderer, BKK_NULL_HANDLE, true, vec4(0.0f), 
                                     "OpaquePass", visibleActors, count, 
                                     renderer.getRenderCompleteSemaphore(), 
                                     &commandBuffers, 4);

      for (uint32_t i(0); i < 4; ++i)
        commandBuffers[i].submitAndRelease();
      
      delete[] commandBuffers;
    }
    else
    {
      command_buffer_t renderSceneCmd(&renderer, "Render");
      renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
      renderSceneCmd.render(visibleActors, count, "OpaquePass");
      renderSceneCmd.submitAndRelease();
    }

    presentFrame();
  }

private:
  free_camera_controller_t cameraController_;
  std::vector<render::texture_t> textures_;
};

int main()
{  
  multithreading_sample_t(uvec2(1200u, 800u)).run();
  return 0;
}

