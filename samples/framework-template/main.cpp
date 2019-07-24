/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/mesh.h"
#include "core/maths.h"

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::core::maths;
using namespace bkk::framework;

class framework_template_t : public application_t
{
public:
  framework_template_t(const uvec2& imageSize)
    :application_t("Framework template", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(0.0f,3.0f,12.0f), vec2(0.1f,0.0f), 0.5f, 0.01f)
  {
    renderer_t& renderer = getRenderer();

    mesh_handle_t mesh = renderer.meshCreate("../resources/teapot.obj", mesh::EXPORT_ALL);    
    material_handle_t material = renderer.materialCreate(renderer.shaderCreate("../framework-template/diffuse.shader"));
    material_t* materialPtr = renderer.getMaterial(material);
    materialPtr->setProperty("globals.albedo", vec4(1.0f) );
    materialPtr->setProperty("globals.lightDirection", vec4( normalize(vec3(1.0f,0.0f,1.0f)),0.0f) );
    
    mat4 transform = createTransform(vec3(0.0f), vec3(1.0f), QUAT_UNIT);
    renderer.actorCreate("actor", mesh, material, transform);

    //create camera
    camera_ = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera_, &renderer);
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
      cameraController_.onKey(key);
  }

  void onMouseMove(const vec2& mousePos, const vec2 &mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
      cameraController_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
  }

  void onResize(uint32_t width, uint32_t height)
  {
    mat4 projectionMatrix = perspectiveProjectionMatrix(1.2f, (f32)width / (f32)height, 0.1f, 100.0f);
    getRenderer().getCamera(camera_)->setProjectionMatrix(projectionMatrix);
  }

  void render()
  {
    renderer_t& renderer = getRenderer();

    beginFrame();
    renderer.setupCamera(camera_);

    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera_, &visibleActors);

    command_buffer_t renderSceneCmd(&renderer, "Render");
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submitAndRelease();

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::LabelText("", "This is a template");
    ImGui::End(); 
  }

private:

  camera_handle_t camera_;
  free_camera_controller_t cameraController_;
};

int main()
{
  framework_template_t sample(uvec2(1200u, 800u));
  sample.loop();

  return 0;
}

