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

class area_lights_sample_t : public application_t
{
public:
  area_lights_sample_t()
    :application_t("Area lights", 1200u, 800u, 3u),
    cameraController_(vec3(0.0f, 4.0f, 12.0f), vec2(0.1f, 0.0f), 1.0f, 0.01f),
    animateLights_(true),
    lightAngle_(0.0f)
  {
    uvec2 imageSize(1200u, 800u);
    renderer_t& renderer = getRenderer();

    //create scene framebuffer
    render_target_handle_t albedoRoughnessRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R8G8B8A8_UNORM, true);
    render_target_handle_t emissionRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R8G8B8A8_UNORM, true);
    render_target_handle_t normalDepthRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    render_target_handle_t targets[] = { albedoRoughnessRT, emissionRT, normalDepthRT };
    sceneFBO_ = renderer.frameBufferCreate(targets, 3u);

    //create meshes
    mesh_handle_t nymph = renderer.meshCreate("../resources/lucy.obj", mesh::EXPORT_ALL);
    mesh_handle_t plane = renderer.meshAdd(mesh::unitQuad(getRenderContext()));

    mesh_handle_t lineLight = renderer.meshAdd(mesh::unitCube(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer.shaderCreate("../area-lights/simple.shader");
    material_handle_t nymphMaterial = renderer.materialCreate(shader);
    renderer.getMaterial(nymphMaterial)->setProperty("globals.albedo", vec3(1.0f, 1.0f, 1.0f));  
    renderer.getMaterial(nymphMaterial)->setProperty("globals.roughness", 0.5f);

    material_handle_t planeMaterial = renderer.materialCreate(shader);
    renderer.getMaterial(planeMaterial)->setProperty("globals.albedo", vec3(1.0f, 1.0f, 1.0f));
    renderer.getMaterial(planeMaterial)->setProperty("globals.roughness", 0.0f);

    shader_handle_t lineLightShader = renderer.shaderCreate("../area-lights/line-light.shader");
    material_handle_t lineLightMaterial = renderer.materialCreate(lineLightShader);
    material_t* lineLightMaterialPtr = renderer.getMaterial(lineLightMaterial);
    lineLightMaterialPtr->setProperty("globals.colorBegin", vec4(1.0f, 0.0f, 0.0f, 1.0f));
    lineLightMaterialPtr->setProperty("globals.colorEnd", vec4(0.0f, 1.0f, 0.0f, 1.0f));
    lineLightMaterialPtr->setProperty("globals.radius", 20.0f);
    lineLightMaterialPtr->setTexture("albedoRoughnessRT", albedoRoughnessRT);
    lineLightMaterialPtr->setTexture("emissionRT", emissionRT);
    lineLightMaterialPtr->setTexture("normalDepthRT", normalDepthRT);

    //create actors
    mat4 modelTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(0.01f), quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), degreeToRadian(-50.0f)));
    renderer.actorCreate("model", nymph, nymphMaterial, modelTransform);

    mat4 floorTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(20.0f), quaternionFromAxisAngle(vec3(1, 0, 0), degreeToRadian(90.0f)));
    renderer.actorCreate("floor", plane, planeMaterial, floorTransform);

    //Create light
    mat4 lightTransform = createTransform(vec3(-3.0f, -0.3f, 0.5f), vec3(0.1f,0.1f,4.0f), QUAT_UNIT);
    lineLight_ = renderer.actorCreate("lineLight", lineLight, lineLightMaterial, lightTransform);

    //create camera
    camera_ = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 100.0f));
    cameraController_.setCameraHandle(camera_, &renderer);
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
  }

  void animateLights()
  {
    lightAngle_ += 0.004f * getTimeDelta();
    mat4 lineLightTx = createTransform(vec3(-3.0f, -0.3f, 0.5f), 
                                       vec3(0.1f, 0.1f, 4.0f), 
                                       quaternionFromAxisAngle(VEC3_UP, lightAngle_));

    getRenderer().actorSetTransform(lineLight_, lineLightTx);
  }

  void render()
  {
    if(animateLights_)
      animateLights();

    beginFrame();

    renderer_t& renderer = getRenderer();
    renderer.setupCamera(camera_);

    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera_, &visibleActors);

    //Render scene
    command_buffer_t renderSceneCmd(&renderer, "GBuffer pass");
    renderSceneCmd.setFrameBuffer(sceneFBO_);
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submitAndRelease();
   
    //Render lights
    actor_t* allActors = nullptr;
    count = renderer.getAllActors(&allActors);
    command_buffer_t lightPassCmd(&renderer, "Light pass");
    lightPassCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    lightPassCmd.render(allActors, count, "LightPass");
    lightPassCmd.submitAndRelease();
    
    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::Checkbox("Animate Lights", &animateLights_);    
    ImGui::End();
  }

private:

  frame_buffer_handle_t sceneFBO_;

  camera_handle_t camera_;
  free_camera_controller_t cameraController_;

  actor_handle_t lineLight_;

  bool animateLights_;  
  float lightAngle_;  
};

int main()
{
  area_lights_sample_t sample;
  sample.loop();

  return 0;
}

