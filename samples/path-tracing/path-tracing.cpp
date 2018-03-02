/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "../utility.h"

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;

  out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition, 1.0);
    uv = vec2(aTexCoord.x, -aTexCoord.y + 1.0);
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  in vec2 uv;

  layout(binding = 0) uniform sampler2D uTexture;

  layout(location = 0) out vec4 result;

  void main(void)
  {
    vec4 texColor = texture(uTexture, uv);
    vec3 color = texColor.rgb;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    result = vec4(color, 1.0);
  }
)";

using namespace bkk;
using namespace sample_utils;

class path_tracing_sample_t : public application_t
{
public:
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
      vkQueueWaitIdle(context.computeQueue_.handle_);
    }
  }

  void onResize(u32 width, u32 height)
  {
    buildPresentationCommandBuffers();
  }

  void onKeyEvent(window::key_e key, bool pressed)
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
  
  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos, bool buttonPressed)
  {    
    if (buttonPressed)
    {
      camera_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
      updateCameraTransform();
    }    
  }

private:

  void generateScene(u32 sphereCount, const maths::vec3& extents, scene_t* scene)
  {
    scene->sphereCount = sphereCount + 1;  //Sphere count + ground

                                           //Generate 5 materials
    material_t materials[5];
    materials[0].albedo = vec3(0.8f, 0.8f, 0.8f);
    materials[0].roughness = 1.0f;
    materials[0].metalness = 0.0f;
    materials[0].F0 = vec3(0.02f, 0.02f, 0.02f);

    //Copper
    materials[1].albedo = vec3(0.0f, 0.0f, 0.0f);
    materials[1].roughness = 0.01f;
    materials[1].metalness = 1.0f;
    materials[1].F0 = vec3(0.95f, 0.3f, 0.3f);

    //Plastic
    materials[2].albedo = vec3(0.05f, 0.85f, 0.05f);
    materials[2].roughness = 0.1f;
    materials[2].metalness = 0.5f;
    materials[2].F0 = vec3(0.4f, 0.4f, 0.4f);

    //Gold
    materials[3].albedo = vec3(0.0f, 0.0f, 0.0f);
    materials[3].roughness = 0.05f;
    materials[3].metalness = 1.0f;
    materials[3].F0 = vec3(1.00f, 0.71f, 0.29f);

    //Iron
    materials[4].albedo = vec3(0.0f, 0.0f, 0.0f);
    materials[4].roughness = 0.1f;
    materials[4].metalness = 1.0f;
    materials[4].F0 = vec3(0.56f, 0.57f, 0.58f);

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
      scene->sphere[i].material = materials[u32(maths::random(0.0f, 1.0f) * 5.0f)];
    }
  }

  bool createResources()
  {
    render::context_t& context = getRenderContext();

    //Create a full screen quad to display the image
    fullscreenQuadmesh_ = fullScreenQuad(context);
    
    //Create the texture that will be updated by the compute shader
    render::texture2DCreate(context, imageSize_.x, imageSize_.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, render::texture_sampler_t(), &renderedImage_);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &renderedImage_);

    //Create data to be passed to the gpu
    buffer_data_t data;
    data.sampleCount = 0u;
    data.maxBounces = 3u;
    data.imageSize = imageSize_;
    data.camera.tx.setIdentity();
    data.camera.verticalFov = (f32)M_PI_2;
    data.camera.focalDistance = 5.0f;
    data.camera.aperture = 0.075f;
    generateScene(150u, vec3(25.0f, 0.0f, 25.0f), &data.scene);

    //Create scene buffer
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::STORAGE_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&data, sizeof(buffer_data_t),
      nullptr, &sceneBuffer_);

    return true;
  }
  
  void createGraphicsPipeline()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t binding = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage::FRAGMENT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &descriptorSetLayout_);

    //Create pipeline layout  
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, &pipelineLayout_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 2u, 1u, 1u, 0u, 1u, &descriptorPool_);

    //Create descriptor set
    render::descriptor_t descriptor = render::getDescriptor(renderedImage_);
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, &descriptor, &descriptorSet_);

    //Load shaders
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);

    //Create graphics pipeline
    bkk::render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled_ = false;
    pipelineDesc.depthWriteEnabled_ = false;
    pipelineDesc.vertexShader_ = vertexShader_;
    pipelineDesc.fragmentShader_ = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, fullscreenQuadmesh_.vertexFormat_, pipelineLayout_, pipelineDesc, &pipeline_);
  }

  void createComputePipeline()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t bindings[2] = { { render::descriptor_t::type::STORAGE_IMAGE,  0, render::descriptor_t::stage::COMPUTE },
                                                 { render::descriptor_t::type::STORAGE_BUFFER, 1, render::descriptor_t::stage::COMPUTE } };

    render::descriptorSetLayoutCreate(context, bindings, 2u, &computeDescriptorSetLayout_);

    //Create pipeline layout
    render::pipelineLayoutCreate(context, &computeDescriptorSetLayout_, 1u, &computePipelineLayout_);

    //Create descriptor set
    render::descriptor_t descriptors[2] = { render::getDescriptor(renderedImage_), render::getDescriptor(sceneBuffer_) };
    render::descriptorSetCreate(context, descriptorPool_, computeDescriptorSetLayout_, descriptors, &computeDescriptorSet_);

    //Create pipeline
    bkk::render::shaderCreateFromGLSL(context, bkk::render::shader_t::COMPUTE_SHADER, "../path-tracing/path-tracing.comp", &computeShader_);
    computePipeline_.computeShader_ = computeShader_;
    render::computePipelineCreate(context, computePipelineLayout_, &computePipeline_);
  }
  
  void buildPresentationCommandBuffers()
  {
    render::context_t& context = getRenderContext();
    const VkCommandBuffer* commandBuffers;
    uint32_t count = bkk::render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      bkk::render::beginPresentationCommandBuffer(context, i, nullptr);
      bkk::render::graphicsPipelineBind(commandBuffers[i], pipeline_);
      bkk::render::descriptorSetBindForGraphics(commandBuffers[i], pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::draw(commandBuffers[i], fullscreenQuadmesh_);
      render::endPresentationCommandBuffer(context, i);
    }
  }

  void buildComputeCommandBuffer()
  {
    render::context_t& context = getRenderContext();

    //Build compute command buffer
    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, &computeCommandBuffer_);
    render::commandBufferBegin(context, nullptr, nullptr, 0u, computeCommandBuffer_);
    bkk::render::computePipelineBind(computeCommandBuffer_.handle_, computePipeline_);
    bkk::render::descriptorSetBindForCompute(computeCommandBuffer_.handle_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);
    vkCmdDispatch(computeCommandBuffer_.handle_, imageSize_.x / 16, imageSize_.y / 16, 1);
    render::commandBufferEnd(computeCommandBuffer_);
  }

  void updateCameraTransform()
  {
    render::gpuBufferUpdate(getRenderContext(), (void*)&camera_.tx_, offsetof(buffer_data_t, camera), sizeof(mat4), &sceneBuffer_);
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

  free_camera_t camera_;
  maths::uvec2 imageSize_;
  u32 sampleCount_ = 0u;
};


//Entry point
int main()
{
  path_tracing_sample_t sample(1200u, 800u);
  sample.loop();

  return 0;
}
