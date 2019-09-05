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
  area_lights_sample_t(const uvec2& imageSize)
    :application_t("Area lights", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(0.0f, 4.0f, 12.0f), vec2(0.1f, 0.0f), 0.5f, 0.01f),
    lightAngle_(0.0f),
    lightVelocity_(4.0),
    lightColorBegin_(1.0f, 1.0f, 1.0f),
    lightColorEnd_(1.0f, 1.0f, 1.0f),
    modelRoughness_(0.5f),
    modelAlbedo_(0.0f,1.0f,0.0f),
    floorRoughness_(0.0f),
    floorAlbedo_(1.0f, 1.0f, 1.0f)
  {
    renderer_t& renderer = getRenderer();

    //create GBuffer
    render_target_handle_t albedoRoughnessRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R8G8B8A8_UNORM, true);
    render_target_handle_t emissionRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R8G8B8A8_UNORM, false);
    render_target_handle_t normalDepthRT = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    render_target_handle_t targets[] = { albedoRoughnessRT, emissionRT, normalDepthRT };
    gBuffer_ = renderer.frameBufferCreate(targets, 3u);

    //create meshes
    mesh_handle_t model = renderer.meshCreate("../resources/lucy.obj", mesh::EXPORT_NORMALS_UVS);
    mesh_handle_t plane = renderer.meshAdd(mesh::unitQuad(getRenderContext()));
    mesh_handle_t lineLight = renderer.meshAdd(mesh::unitCube(getRenderContext()));

    //create materials
    shader_handle_t shader = renderer.shaderCreate("../area-lights/simple.shader");
    material_handle_t modelMaterial = renderer.materialCreate(shader);
    material_t* modelMaterialPtr = renderer.getMaterial(modelMaterial);
    modelMaterialPtr->setProperty("globals.albedo", modelAlbedo_);
    modelMaterialPtr->setProperty("globals.roughness", modelRoughness_);

    material_handle_t floorMaterial = renderer.materialCreate(shader);
    material_t* floorMaterialPtr = renderer.getMaterial(floorMaterial);
    floorMaterialPtr->setProperty("globals.albedo", floorAlbedo_);
    floorMaterialPtr->setProperty("globals.roughness", 0.0f);

    shader_handle_t lineLightShader = renderer.shaderCreate("../area-lights/line-light.shader");
    material_handle_t lineLightMaterial = renderer.materialCreate(lineLightShader);
    material_t* lineLightMaterialPtr = renderer.getMaterial(lineLightMaterial);
    lineLightMaterialPtr->setProperty("globals.colorBegin", lightColorBegin_);
    lineLightMaterialPtr->setProperty("globals.colorEnd", lightColorEnd_);
    lineLightMaterialPtr->setProperty("globals.radius", 20.0f);
    lineLightMaterialPtr->setTexture("albedoRoughnessRT", albedoRoughnessRT);
    lineLightMaterialPtr->setTexture("emissionRT", emissionRT);
    lineLightMaterialPtr->setTexture("normalDepthRT", normalDepthRT);

    //create actors
    mat4 modelTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(0.01f), quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), degreeToRadian(-50.0f)));
    renderer.actorCreate("model", model, modelMaterial, modelTransform);

    mat4 floorTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(20.0f), quaternionFromAxisAngle(vec3(1, 0, 0), degreeToRadian(90.0f)));
    renderer.actorCreate("floor", plane, floorMaterial, floorTransform);

    //Create line light
    mat4 lightTransform = createTransform(vec3(-3.0f, -0.3f, 0.5f), vec3(0.1f,0.1f,4.0f), QUAT_UNIT);
    renderer.actorCreate("lineLight", lineLight, lineLightMaterial, lightTransform);

    //create camera
    camera_handle_t camera = renderer.cameraAdd(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 100.0f));
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
  
  void animateLights()
  {
    lightAngle_ += (getTimeDelta() * lightVelocity_ / 1000.0f);
    mat4 lineLightTx = createTransform(vec3(-3.0f, -0.3f, 0.5f), 
                                       vec3(0.1f, 0.1f, 4.0f), 
                                       quaternionFromAxisAngle(VEC3_UP, lightAngle_));

    renderer_t& renderer = getRenderer();
    renderer.actorSetTransform(renderer.findActor("lineLight")->getTransformHandle(), lineLightTx);
  }

  void render()
  {
    animateLights();

    beginFrame();
    renderer_t& renderer = getRenderer();

    camera_handle_t camera = cameraController_.getCameraHandle();
    renderer.setupCamera(camera);

    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera, &visibleActors);

    //Geometry pass
    command_buffer_t renderSceneCmd(&renderer, "Geometry pass");
    renderSceneCmd.setFrameBuffer(gBuffer_);
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submitAndRelease();
   
    //Render line light
    command_buffer_t lightPassCmd(&renderer, "Light pass", renderer.getRenderCompleteSemaphore());
    lightPassCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    lightPassCmd.render(renderer.findActor("lineLight"), 1u, "LightPass");
    lightPassCmd.submitAndRelease();
    
    presentFrame();
  }

  void buildGuiFrame()
  {
    renderer_t& renderer = getRenderer();

    ImGui::Begin("Controls");

    ImGui::LabelText("", "Light");
    ImGui::SliderFloat("Light velocity (rad/s)", &lightVelocity_, 0.0f, 10.0f);
    ImGui::ColorEdit3("Light color begin", lightColorBegin_.data);
    ImGui::ColorEdit3("Light color end", lightColorEnd_.data);
    material_t* lightMaterial = renderer.getMaterial(renderer.findActor("lineLight")->getMaterialHandle());
    lightMaterial->setProperty("globals.colorBegin", lightColorBegin_);
    lightMaterial->setProperty("globals.colorEnd", lightColorEnd_);

    ImGui::LabelText("", "Model");
    ImGui::ColorEdit3("Model albedo", modelAlbedo_.data);
    ImGui::SliderFloat("Model roughness", &modelRoughness_, 0.0f, 1.0f);
    material_t* modelMaterial = renderer.getMaterial(renderer.findActor("model")->getMaterialHandle());
    modelMaterial->setProperty("globals.albedo", modelAlbedo_);
    modelMaterial->setProperty("globals.roughness", modelRoughness_);

    ImGui::LabelText("", "Floor");
    ImGui::ColorEdit3("Floor albedo", floorAlbedo_.data);
    ImGui::SliderFloat("Floor roughness", &floorRoughness_, 0.0f, 1.0f);
    material_t* floorMaterial = renderer.getMaterial(renderer.findActor("floor")->getMaterialHandle());
    floorMaterial->setProperty("globals.albedo", floorAlbedo_);
    floorMaterial->setProperty("globals.roughness", floorRoughness_);

    ImGui::End();
  }

private:

  frame_buffer_handle_t gBuffer_;
  free_camera_controller_t cameraController_;

  float lightAngle_;  
  float lightVelocity_;
  vec3 lightColorBegin_;
  vec3 lightColorEnd_;

  float modelRoughness_;
  vec3 modelAlbedo_;

  float floorRoughness_;
  vec3 floorAlbedo_;
};

int main()
{
  area_lights_sample_t(uvec2(1200u, 800u) ).run();
  return 0;
}

