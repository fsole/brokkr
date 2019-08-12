/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/application.h"
#include "framework/camera.h"

#include "core/render.h"
#include "core/window.h"
#include "core/image.h"
#include "core/mesh.h"
#include "core/maths.h"

using namespace bkk;
using namespace bkk::core;
using namespace bkk::core::maths;

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;
  layout(location = 0)out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition, 1.0);
    uv = vec2(aTexCoord.x, -aTexCoord.y + 1.0);
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  layout(binding = 0) uniform sampler2D uTexture;
  layout(location = 0) in vec2 uv;  
  layout(location = 0) out vec4 result;

  void main(void)
  {
    vec4 texColor = texture(uTexture, uv);
    vec3 color = texColor.rgb;
    color = pow(color, vec3(1.0 / 2.2));
    result = vec4(color, 1.0);
  }
)";

class path_tracing_sample_t : public framework::application_t
{
public:
  
  path_tracing_sample_t( u32 width, u32 height )
  :application_t("Path tracing", width, height, 3u ),
  imageSize_(width, height)
  {
    createResources();
    createGraphicsPipeline();
    createComputePipeline();
    buildPresentationCommandBuffers();
    buildComputeCommandBuffer();
  }

  void onQuit()
  {
    render::context_t& context = getRenderContext();
    
    mesh::destroy(context, &fullscreenQuadmesh_);
    render::textureDestroy(context, &renderedImage_);
    render::gpuBufferDestroy(context, nullptr, &sceneBuffer_);

    render::shaderDestroy(context, &vertexShader_);
    render::shaderDestroy(context, &fragmentShader_);
    render::shaderDestroy(context, &computeShader_);

    render::descriptorSetDestroy(context, &descriptorSet_);
    render::descriptorSetLayoutDestroy(context, &descriptorSetLayout_);    
    render::pipelineLayoutDestroy(context, &pipelineLayout_);
    render::graphicsPipelineDestroy(context, &pipeline_);

    render::descriptorSetDestroy(context, &computeDescriptorSet_);
    render::descriptorSetLayoutDestroy(context, &computeDescriptorSetLayout_);    
    render::computePipelineDestroy(context, &computePipeline_);
    render::pipelineLayoutDestroy(context, &computePipelineLayout_);
    render::commandBufferDestroy(context, &computeCommandBuffer_);

    render::descriptorPoolDestroy(context, &descriptorPool_);
  }

  void render()
  {
    render::context_t& context = getRenderContext();
    render::presentFrame(&context);

    if (sampleCount_ < 1000.0f)
    {
      //Submit compute command buffer
      render::gpuBufferUpdate(context, (void*)&sampleCount_, 0, sizeof(u32), &sceneBuffer_);
      ++sampleCount_;

      render::commandBufferSubmit(context, computeCommandBuffer_);
      vkQueueWaitIdle(context.computeQueue.handle);
    }
  }

  void onResize(u32 width, u32 height)
  {
    buildPresentationCommandBuffers();
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
    {
      switch (key)
      {
      case window::key_e::KEY_UP:
      case 'w':
      {
        camera_.Move(0.0f, -0.5f);
        updateCameraTransform();
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        camera_.Move(0.0f, 0.5f);
        updateCameraTransform();
        break;
      }
      case window::key_e::KEY_LEFT:
      case 'a':
      {
        camera_.Move(-0.5f, 0.0f);
        updateCameraTransform();
        break;
      }
      case window::key_e::KEY_RIGHT:
      case 'd':
      {
        camera_.Move(0.5f, 0.0f);
        updateCameraTransform();
        break;
      }
      default:
        break;
      }
    }
  }
  
  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos)
  {
    if (getMousePressedButton() > -1)
    {
      camera_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
      updateCameraTransform();
    }    
  }

private:

  struct camera_t
  {
    maths::mat4 tx;
    f32 verticalFov;
    f32 focalDistance;
    f32 aperture;
    f32 padding;
  };

  struct material_t
  {
    maths::vec3 albedo;
    float metalness;
    maths::vec3 F0;
    float roughness;
  };

  struct sphere_t
  {
    maths::vec3 origin;
    float radius;
    material_t material;
  };

  struct scene_t
  {
    u32 sphereCount;
    u32 padding[3];
    sphere_t sphere[200];
  };

  struct buffer_data_t
  {
    u32 sampleCount;
    u32 maxBounces;
    maths::uvec2 imageSize;
    camera_t camera;
    scene_t scene;
  };

  void generateScene(u32 sphereCount, const maths::vec3& extents, scene_t* scene)
  {
    scene->sphereCount = sphereCount + 1;  //Sphere count + ground

    //Generate 5 materials
    material_t materials[6];
    materials[0].albedo = vec3(1.8f, 1.8f, 1.8f);
    materials[0].roughness = 1.0f;
    materials[0].metalness = 0.0f;
    materials[0].F0 = vec3(0.2f, 0.2f, 0.2f);

    materials[1].albedo = vec3(1.8f, 0.5f, 0.5f);
    materials[1].roughness = 1.0f;
    materials[1].metalness = 0.0f;
    materials[1].F0 = vec3(0.2f, 0.2f, 0.2f);

    materials[2].albedo = vec3(0.05f, 0.85f, 0.05f);
    materials[2].roughness = 0.1f;
    materials[2].metalness = 0.5f;
    materials[2].F0 = vec3(0.4f, 0.4f, 0.4f);

    materials[3].albedo = vec3(0.0f, 0.0f, 0.0f);
    materials[3].roughness = 0.05f;
    materials[3].metalness = 1.0f;
    materials[3].F0 = vec3(1.022f, 0.782f, 0.344f);

    materials[4].albedo = vec3(0.0f, 0.0f, 0.0f);
    materials[4].roughness = 0.1f;
    materials[4].metalness = 1.0f;
    materials[4].F0 = vec3(0.56f, 0.56f, 0.57f);

    materials[5].albedo = vec3(0.2f, 0.2f, 1.8f);
    materials[5].roughness = 1.0f;
    materials[5].metalness = 0.0f;
    materials[5].F0 = vec3(0.2f, 0.2f, 0.2f);

    //Ground
    scene->sphere[0].origin = maths::vec3(0.0f, -100000.0f, 0.0f);
    scene->sphere[0].radius = 100000.0f - 1.0f;
    scene->sphere[0].material = materials[0];

    for (u32 i(1); i<sphereCount + 1; ++i)
    {
      bool ok(false);
      f32 radius;
      vec3 center;
      do
      {
        ok = true;
        radius = maths::random(0.0f, 1.0f) + 0.4f;
        center = vec3((2.0f * maths::random(0.0f, 1.0f) - 1.0f) * extents.x, radius - 1.0001f, (2.0f * maths::random(0.0f, 1.0f) - 1.0f) * extents.z);
        for (u32 j(0); j<i; ++j)
        {
          f32 distance = length(center - scene->sphere[j].origin);
          if (distance < radius + scene->sphere[j].radius)
          {
            ok = false;
            break;
          }
        }
      } while (!ok);

      scene->sphere[i].radius = radius;
      scene->sphere[i].origin = center;
      scene->sphere[i].material = materials[u32(maths::random(0.0f, 1.0f) * 6.0f)];
    }
  }

  bool createResources()
  {
    render::context_t& context = getRenderContext();

    //Create a full screen quad to display the image
    fullscreenQuadmesh_ = mesh::fullScreenQuad(context);
    
    //Create the texture that will be updated by the compute shader
    render::texture2DCreate(context, imageSize_.x, imageSize_.y, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, render::texture_sampler_t(), &renderedImage_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_GENERAL, &renderedImage_);

    //Create data to be passed to the gpu
    buffer_data_t data;
    data.sampleCount = 0u;
    data.maxBounces = 3u;
    data.imageSize = imageSize_;
    data.camera.tx.setIdentity();
    data.camera.verticalFov = (f32)PI_2;
    data.camera.focalDistance = 5.0f;
    data.camera.aperture = 0.075f;
    generateScene(150u, vec3(25.0f, 0.0f, 25.0f), &data.scene);

    //Create scene buffer
    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&data, sizeof(buffer_data_t),
      nullptr, &sceneBuffer_);

    return true;
  }
  
  void createGraphicsPipeline()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t binding = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage_e::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &descriptorSetLayout_);

    //Create pipeline layout  
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, nullptr, 0u, &pipelineLayout_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 2u,
      render::combined_image_sampler_count(1u),
      render::uniform_buffer_count(0u),
      render::storage_buffer_count(1u),
      render::storage_image_count(1u),
      &descriptorPool_);

    //Create descriptor set
    render::descriptor_t descriptor = render::getDescriptor(renderedImage_);
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, &descriptor, &descriptorSet_);

    //Load shaders
    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);

    //Create graphics pipeline
    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    pipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    pipelineDesc.blendState.resize(1);
    pipelineDesc.blendState[0].colorWriteMask = 0xF;
    pipelineDesc.blendState[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled = false;
    pipelineDesc.depthWriteEnabled = false;
    pipelineDesc.vertexShader = vertexShader_;
    pipelineDesc.fragmentShader = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain.renderPass, 0u, fullscreenQuadmesh_.vertexFormat, pipelineLayout_, pipelineDesc, &pipeline_);
  }

  void createComputePipeline()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t bindings[2] = { { render::descriptor_t::type_e::STORAGE_IMAGE,  0, render::descriptor_t::stage_e::COMPUTE },
                                                 { render::descriptor_t::type_e::STORAGE_BUFFER, 1, render::descriptor_t::stage_e::COMPUTE } };

    render::descriptorSetLayoutCreate(context, bindings, 2u, &computeDescriptorSetLayout_);

    //Create pipeline layout
    render::pipelineLayoutCreate(context, &computeDescriptorSetLayout_, 1u, nullptr, 0u, &computePipelineLayout_);

    //Create descriptor set
    render::descriptor_t descriptors[2] = { render::getDescriptor(renderedImage_), render::getDescriptor(sceneBuffer_) };
    render::descriptorSetCreate(context, descriptorPool_, computeDescriptorSetLayout_, descriptors, &computeDescriptorSet_);

    //Create pipeline
    render::shaderCreateFromGLSL(context, render::shader_t::COMPUTE_SHADER, "../path-tracing/path-tracing.comp", &computeShader_);    
    render::computePipelineCreate(context, computePipelineLayout_, computeShader_, &computePipeline_);
  }
  
  void buildPresentationCommandBuffers()
  {
    render::context_t& context = getRenderContext();
    const render::command_buffer_t* commandBuffers;
    uint32_t count = render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      render::beginPresentationCommandBuffer(context, i, nullptr);
      render::graphicsPipelineBind(commandBuffers[i], pipeline_);
      render::descriptorSetBind(commandBuffers[i], pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::draw(commandBuffers[i], fullscreenQuadmesh_);
      render::endPresentationCommandBuffer(context, i);
    }
  }

  void buildComputeCommandBuffer()
  {
    render::context_t& context = getRenderContext();

    //Build compute command buffer

    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, VK_NULL_HANDLE, &computeCommandBuffer_);
    render::commandBufferBegin(context, computeCommandBuffer_);
    render::computePipelineBind(computeCommandBuffer_, computePipeline_);
    render::descriptorSetBind(computeCommandBuffer_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);
    render::computeDispatch(computeCommandBuffer_, (imageSize_.x + 15) / 16, (imageSize_.y + 15) / 16, 1);
    render::commandBufferEnd(computeCommandBuffer_);
  }

  void updateCameraTransform()
  {
    render::gpuBufferUpdate(getRenderContext(), (void*)&camera_.getWorldMatrix(), offsetof(buffer_data_t, camera), sizeof(mat4), &sceneBuffer_);
    sampleCount_ = 0;
  }

private:
    
  render::texture_t renderedImage_;
  mesh::mesh_t fullscreenQuadmesh_;

  render::descriptor_pool_t descriptorPool_;

  render::pipeline_layout_t pipelineLayout_;
  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_t descriptorSet_;
  render::graphics_pipeline_t pipeline_;


  render::pipeline_layout_t computePipelineLayout_;
  render::descriptor_set_layout_t computeDescriptorSetLayout_;
  render::descriptor_set_t computeDescriptorSet_;
  render::compute_pipeline_t computePipeline_;
  render::command_buffer_t computeCommandBuffer_;

  render::gpu_buffer_t sceneBuffer_;
  
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;
  render::shader_t computeShader_;

  framework::free_camera_controller_t camera_;
  maths::uvec2 imageSize_;
  u32 sampleCount_ = 0u;
};


//Entry point
int main()
{
  path_tracing_sample_t sample(1200u, 800u);
  sample.run();

  return 0;
}
