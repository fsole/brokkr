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

static bool loadDDS(const char* path, image::image2D_t* image)
{
  struct DDS_PIXELFORMAT
  {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
  };

  struct DDS_HEADER
  {
    uint32_t        dwSize;
    uint32_t        dwFlags;
    uint32_t        dwHeight;
    uint32_t        dwWidth;
    uint32_t        dwPitchOrLinearSize;
    uint32_t        dwDepth;
    uint32_t        dwMipMapCount;
    uint32_t        dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        dwCaps;
    uint32_t        dwCaps2;
    uint32_t        dwCaps3;
    uint32_t        dwCaps4;
    uint32_t        dwReserved2;
  };

  struct DDS_HEADER_DXT10
  {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
  };

  enum
  {
    DDS_FORMAT_R32G32B32A32_FLOAT = 2,
    DDS_FORMAT_R32G32_FLOAT = 16
  };

  FILE* file = fopen(path, "rb");

  if (!file)
    return false;
    
  uint32_t magicNumber = 0u;
  fread(&magicNumber, sizeof(uint32_t), 1, file);
  if (magicNumber != 0x20534444)
    return false;

  DDS_HEADER header = {};
  fread(&header, sizeof(header), 1, file);

  DDS_HEADER_DXT10 headerDX10 = {};
  fread(&headerDX10, sizeof(headerDX10), 1, file);
   
  //Read pixel data
  uint32_t size = header.dwPitchOrLinearSize * header.dwHeight;  
  uint8_t* data =(uint8_t*)malloc(size);
  bool success = fread(data, size, 1, file) == 1;
  fclose(file);

  if( success )
  {
    image->width = header.dwWidth;
    image->height = header.dwHeight;
    image->componentCount = 4;
    image->componentSize = 4;
    image->dataSize = 16 * header.dwWidth * header.dwHeight;
    image->data = data;

    if (headerDX10.dxgiFormat == DDS_FORMAT_R32G32_FLOAT)
    {
      //Add missing channels
      image->data = (uint8_t*)malloc(image->dataSize);
      float* imageDataPtr = (float*)image->data;
      float* dataPtr = (float*)data;
      for (int i(0); i < image->width*image->height; ++i)
        for (int component(0); component < 4; ++component)
          imageDataPtr[4 * i + component] = component < 4 ? dataPtr[2*i + component] : 0.0f;

      free(data);
    }
  }
  else
  {
    free(data);
  }

  return success;
}

class area_lights_sample_t : public application_t
{
public:
  area_lights_sample_t(const uvec2& imageSize)
    :application_t("Area lights", imageSize.x, imageSize.y, 3u),
    cameraController_(vec3(0.0f, 4.0f, 12.0f), vec2(0.1f, 0.0f), 0.5f, 0.01f),
    lineLightAngle_(0.0f),
    lineLightVelocity_(4.0f),
    lineLightColorBegin_(1.0f, 1.0f, 1.0f),
    lineLightColorEnd_(1.0f, 1.0f, 1.0f),
    lineLightLength_(4.0f),
    areaLightVelocity_(4.0f),
    areaLightAngle_(0.0f),
    areaLightColor_(1.0f,0.0f,0.0f),
    areaLightScale_(3.5f),
    modelRoughness_(0.5f),
    modelAlbedo_(1.0f,1.0f,1.0f),
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

    //Create buffer where lighting will be applied
    resultImage_ = renderer.renderTargetCreate(imageSize.x, imageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, false);
    resultFBO_ = renderer.frameBufferCreate(&resultImage_, 1u);

    //Material for final gamma correction
    shader_handle_t shader = renderer.shaderCreate("../area-lights/blit-gamma-correct.shader");
    blitMaterial_ = renderer.materialCreate(shader);

    //Create and setup model material
    shader = renderer.shaderCreate("../area-lights/simple.shader");
    material_handle_t modelMaterial = renderer.materialCreate(shader);
    material_t* modelMaterialPtr = renderer.getMaterial(modelMaterial);
    modelMaterialPtr->setProperty("globals.albedo", modelAlbedo_);
    modelMaterialPtr->setProperty("globals.roughness", modelRoughness_);

    //Create and setup floor material
    material_handle_t floorMaterial = renderer.materialCreate(shader);
    material_t* floorMaterialPtr = renderer.getMaterial(floorMaterial);
    floorMaterialPtr->setProperty("globals.albedo", floorAlbedo_);
    floorMaterialPtr->setProperty("globals.roughness", 0.0f);

    //Create and setup line light material
    shader_handle_t lineLightShader = renderer.shaderCreate("../area-lights/line-light.shader");
    material_handle_t lineLightMaterial = renderer.materialCreate(lineLightShader);
    material_t* lineLightMaterialPtr = renderer.getMaterial(lineLightMaterial);
    lineLightMaterialPtr->setProperty("globals.colorBegin", lineLightColorBegin_);
    lineLightMaterialPtr->setProperty("globals.colorEnd", lineLightColorEnd_);
    lineLightMaterialPtr->setProperty("globals.radius", 50.0f);
    lineLightMaterialPtr->setTexture("albedoRoughnessRT", albedoRoughnessRT);
    lineLightMaterialPtr->setTexture("emissionRT", emissionRT);
    lineLightMaterialPtr->setTexture("normalDepthRT", normalDepthRT);

    //Create and setup area light material
    shader_handle_t areaLightShader = renderer.shaderCreate("../area-lights/area-light.shader");
    material_handle_t areaLightMaterial = renderer.materialCreate(areaLightShader);
    material_t* areaLightMaterialPtr = renderer.getMaterial(areaLightMaterial);
    ltcAmpTexture_ = textureFromDDS("../area-lights/ltc_amp.dds");
    ltcMatTexture_ = textureFromDDS("../area-lights/ltc_mat.dds");
    areaLightMaterialPtr->setProperty("globals.color", areaLightColor_);
    areaLightMaterialPtr->setProperty("globals.radius", 50.0f);
    areaLightMaterialPtr->setTexture("albedoRoughnessRT", albedoRoughnessRT);
    areaLightMaterialPtr->setTexture("emissionRT", emissionRT);
    areaLightMaterialPtr->setTexture("normalDepthRT", normalDepthRT);
    areaLightMaterialPtr->setTexture("ltcAmpTexture", ltcAmpTexture_);
    areaLightMaterialPtr->setTexture("ltcMatTexture", ltcMatTexture_);

    //create scene actors
    mesh_handle_t modelMesh = renderer.meshCreate("../resources/lucy.obj", mesh::EXPORT_NORMALS_UVS);
    mat4 modelTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(0.01f), quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), degreeToRadian(-50.0f)));
    renderer.actorCreate("model", modelMesh, modelMaterial, modelTransform);

    mesh_handle_t planeMesh = renderer.meshAdd(mesh::unitQuad(getRenderContext()));
    mat4 floorTransform = createTransform(vec3(0.0f, -1.0f, 0.0f), vec3(20.0f), quaternionFromAxisAngle(vec3(1, 0, 0), degreeToRadian(90.0f)));
    renderer.actorCreate("floor", planeMesh, floorMaterial, floorTransform);

    //Create light actors
    mesh_handle_t cubeMesh = renderer.meshAdd(mesh::unitCube(getRenderContext()));
    lights_[0] = renderer.actorCreate("lineLight", cubeMesh, lineLightMaterial, mat4());
    lights_[1] = renderer.actorCreate("areaLight", cubeMesh, areaLightMaterial, mat4());

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

  void onQuit()
  {
    render::context_t& context = getRenderer().getContext();
    render::textureDestroy(context, &ltcAmpTexture_);
    render::textureDestroy(context, &ltcMatTexture_);
  }
  
  void animateLights()
  {
    lineLightAngle_ += (getTimeDelta() * lineLightVelocity_ / 1000.0f);
    mat4 lineLightTx = createTransform(vec3(-3.0f, -0.3f, 0.5f), 
                                       vec3(0.1f, 0.1f, lineLightLength_),
                                       quaternionFromAxisAngle(VEC3_UP, lineLightAngle_));

    renderer_t& renderer = getRenderer();
    renderer.actorSetTransform(lights_[0], lineLightTx);
    
    areaLightAngle_ += (getTimeDelta() * areaLightVelocity_ / 1000.0f);
    mat4 areaLightTx = createTransform(vec3(2.0f, 2.0f, 5.0f),
      vec3(areaLightScale_, areaLightScale_, 0.0f),
      quaternionFromAxisAngle( VEC3_UP, areaLightAngle_));

    renderer.actorSetTransform(lights_[1], areaLightTx);
  }

  render::texture_t textureFromDDS(const char* path)
  {
    render::texture_t texture;
    image::image2D_t image = {};
    if (loadDDS(path, &image))
    { 
      render::texture_sampler_t sampler = { render::texture_sampler_t::filter_mode_e::LINEAR,
                                            render::texture_sampler_t::filter_mode_e::LINEAR,
                                            render::texture_sampler_t::filter_mode_e::LINEAR,
                                            render::texture_sampler_t::wrap_mode_e::CLAMP_TO_EDGE,
                                            render::texture_sampler_t::wrap_mode_e::CLAMP_TO_EDGE,
                                            render::texture_sampler_t::wrap_mode_e::CLAMP_TO_EDGE };

      render::texture2DCreate(getRenderer().getContext(), &image, 1u, sampler, &texture);
      image::free(&image);
    }
    return texture;
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
   
    //Render lights
    actor_t lights[] = { *renderer.getActor(lights_[0]), *renderer.getActor(lights_[1]) };
    command_buffer_t lightPassCmd(&renderer, "Light pass");
    lightPassCmd.setFrameBuffer(resultFBO_);
    lightPassCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));    
    lightPassCmd.render(lights, 2u, "LightPass");
    lightPassCmd.submitAndRelease();

    //Gamma correction
    command_buffer_t blitToBackBuffer(&renderer, "Gamma correction", renderer.getRenderCompleteSemaphore());
    blitToBackBuffer.blit(resultImage_, blitMaterial_);
    blitToBackBuffer.submitAndRelease();

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");

    ImGui::LabelText("", "Line Light");
    ImGui::SliderFloat("Line light velocity (rad/s)", &lineLightVelocity_, 0.0f, 10.0f);
    ImGui::ColorEdit3("Line light color begin", lineLightColorBegin_.data);
    ImGui::ColorEdit3("Line light color end", lineLightColorEnd_.data);
    ImGui::SliderFloat("Line light length", &lineLightLength_, 0.0f, 10.0f);
    material_t* materialPtr = getRenderer().getActorMaterial(lights_[0]);
    materialPtr->setProperty("globals.colorBegin", lineLightColorBegin_);
    materialPtr->setProperty("globals.colorEnd", lineLightColorEnd_);

    ImGui::Separator();

    ImGui::LabelText("", "Area Light");
    ImGui::SliderFloat("Area light velocity (rad/s)", &areaLightVelocity_, 0.0f, 10.0f);
    ImGui::ColorEdit3("Area light color", areaLightColor_.data);
    ImGui::SliderFloat("Area light scale", &areaLightScale_, 0.0f, 5.0f);
    materialPtr = getRenderer().getActorMaterial(lights_[1]);
    materialPtr->setProperty("globals.color", areaLightColor_);

    ImGui::Separator();

    ImGui::LabelText("", "Model");
    ImGui::ColorEdit3("Model Albedo", modelAlbedo_.data);
    ImGui::SliderFloat("Model Roughness", &modelRoughness_, 0.0f, 1.0f);
    material_t* modelMaterial = getRenderer().getMaterial(getRenderer().findActor("model")->getMaterialHandle());
    modelMaterial->setProperty("globals.albedo", modelAlbedo_);
    modelMaterial->setProperty("globals.roughness", modelRoughness_);

    ImGui::Separator();

    ImGui::LabelText("", "Floor");
    ImGui::ColorEdit3("Floor Albedo", floorAlbedo_.data);
    ImGui::SliderFloat("Floor Roughness", &floorRoughness_, 0.0f, 1.0f);
    material_t* floorMaterial = getRenderer().getMaterial(getRenderer().findActor("floor")->getMaterialHandle());
    floorMaterial->setProperty("globals.albedo", floorAlbedo_);
    floorMaterial->setProperty("globals.roughness", floorRoughness_);

    ImGui::End();
  }

private:

  frame_buffer_handle_t gBuffer_;
  free_camera_controller_t cameraController_;

  frame_buffer_handle_t resultFBO_;
  render_target_handle_t resultImage_;
  material_handle_t blitMaterial_;

  float lineLightAngle_;
  float lineLightVelocity_;
  vec3 lineLightColorBegin_;
  vec3 lineLightColorEnd_;
  float lineLightLength_;

  float areaLightVelocity_;
  float areaLightAngle_;
  vec3 areaLightColor_;
  float areaLightScale_;
  render::texture_t ltcAmpTexture_;
  render::texture_t ltcMatTexture_;
  actor_handle_t lights_[2];

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

