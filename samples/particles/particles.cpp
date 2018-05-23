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

#include "application.h"
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "timer.h"
#include "camera.h"

using namespace bkk;
using namespace maths;

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec3 aNormal;
  layout(location = 2) in vec2 uv;
  /*layout(location = 3) in vec3 instancePosition;
  layout(location = 4) in float instanceScale;
  layout(location = 5) in vec3 instanceRotation;
  layout(location = 6) in float instanceOpacity;*/

  struct particle_t
  {
    vec3 position;
    float scale;
    vec3 angle;
    float opacity;
  };

  layout(binding = 1)  readonly buffer PARTICLES
  {
    particle_t data[];
  }particles;

  layout(binding = 0) uniform UNIFORMS
  {
    mat4 modelView;
    mat4 modelViewProjection;
  }uniforms;

  layout(location = 0) out vec4 color;

  mat3 rotationFromEuler( vec3 eulerAngles )
  {
    mat3 mx;
	  float s = sin(eulerAngles.x);
	  float c = cos(eulerAngles.x);
	  mx[0] = vec3(c, s, 0.0);
	  mx[1] = vec3(-s, c, 0.0);
	  mx[2] = vec3(0.0, 0.0, 1.0);
	
    mat3 my;
	  s = sin(eulerAngles.y);
	  c = cos(eulerAngles.y);
	  my[0] = vec3(c, 0.0, s);
	  my[1] = vec3(0.0, 1.0, 0.0);
	  my[2] = vec3(-s, 0.0, c);
	
	  mat3 mz;
	  s = sin(eulerAngles.z);
	  c = cos(eulerAngles.z);		
	  mz[0] = vec3(1.0, 0.0, 0.0);
	  mz[1] = vec3(0.0, c, s);
	  mz[2] = vec3(0.0, -s, c);
	
	  return mz * my * mx;
  }

  void main(void)
  {   
    mat3 rotation = rotationFromEuler(particles.data[gl_InstanceIndex].angle);
    vec3 localPosition = aPosition.xyz * rotation;
    gl_Position = uniforms.modelViewProjection * vec4((localPosition * particles.data[gl_InstanceIndex].scale) + particles.data[gl_InstanceIndex].position, 1.0);
    color = vec4(particles.data[gl_InstanceIndex].opacity);
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec4 color;
  layout(location = 0) out vec4 result;  
  void main(void)
  {
    result = color;
  }
)";

static const char* gComputeShader = R"(
  #version 440 core
  #extension GL_ARB_separate_shader_objects : enable
  #extension GL_ARB_shading_language_420pack : enable
  layout (local_size_x = 64, local_size_y = 1) in;
  struct particle_t
  {
    vec3 position;
    float scale;
    vec3 angle;
    float opacity;
  };
  layout (std140, binding = 0) buffer SSBO
  {
    particle_t particle[];  
  }data;

  layout(push_constant) uniform PushConstants
  {
	  layout (offset = 0) uint particleCount;
  }pushConstants;

  void main()
  {
    uint particleIndex = gl_GlobalInvocationID.x;
    if( particleIndex < pushConstants.particleCount )
    {
      data.particle[particleIndex].angle += vec3(0.05,0.05,0.05);
    }
  }
)";

class particles_sample_t : public application_t
{
public:
  particles_sample_t()
  :application_t("Particles", 1200u, 800u, 3u),
   particleCount_( 1000 ),
   camera_(25.0f, vec2(0.0f, 0.0f), 0.01f)
  {
    render::context_t& context = getRenderContext();

    projectionTx_ = perspectiveProjectionMatrix(1.5f, getWindow().width_ / (float)getWindow().height_, 1.0f, 1000.0f);
    modelTx_ = createTransform(vec3(0.0, 0.0, 0.0), VEC3_ONE, QUAT_UNIT);

    //Create uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&matrices, sizeof(matrices),
      nullptr, &globalUnifomBuffer_);

    mesh_ = mesh::unitQuad(context);
    

    struct particle
    {
      vec3 position;
      float scale;
      vec3 angle;
      float opacity;
    };

    std::vector<particle> particles(particleCount_);
    for (u32 i(0); i < particleCount_; ++i)
    {
      particles[i].position = vec3(maths::random(-10.0f, 10.0f), maths::random(-10.0f, 10.0f), maths::random(-10.0f, 10.0f));
      particles[i].scale = maths::random(0.25f, 1.0f);
      particles[i].angle = vec3(maths::random(0.0f, 3.14f), maths::random(0.0f, 3.14f), maths::random(0.0f, 3.14f));
      particles[i].opacity = maths::random(0.0f, 1.0f);
    }

    u32 usage = render::gpu_buffer_t::usage::STORAGE_BUFFER;
    render::gpuBufferCreate(context, usage,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)particles.data(), sizeof(particle)*particleCount_,
      nullptr, &particleBuffer_);

    //Create pipeline and descriptor set layouts
    render::descriptor_binding_t bindings[2] = { { render::descriptor_t::type::UNIFORM_BUFFER, 0u, render::descriptor_t::stage::VERTEX },
                                                 { render::descriptor_t::type::STORAGE_BUFFER, 1u, render::descriptor_t::stage::VERTEX } };

    render::descriptorSetLayoutCreate(context, bindings, 2u, &descriptorSetLayout_);
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, nullptr, 0u, &pipelineLayout_);

    //Create descriptor set
    render::descriptorPoolCreate(context, 2u,
      render::combined_image_sampler_count(0u),
      render::uniform_buffer_count(1u),
      render::storage_buffer_count(1u),
      render::storage_image_count(0u),
      &descriptorPool_);

    render::descriptor_t descriptors[2] = { render::getDescriptor(globalUnifomBuffer_), render::getDescriptor(particleBuffer_) };
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, descriptors, &descriptorSet_);


    //Create pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);
    bkk::render::graphics_pipeline_t::description_t pipelineDesc;
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_NONE;
    pipelineDesc.depthTestEnabled_ = true;
    pipelineDesc.depthWriteEnabled_ = true;
    pipelineDesc.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader_ = vertexShader_;
    pipelineDesc.fragmentShader_ = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh_.vertexFormat_, pipelineLayout_, pipelineDesc, &pipeline_);

    buildCommandBuffers();

    createComputePipeline();
    buildComputeCommandBuffer();
  }

  void onQuit()
  {
    render::context_t& context = getRenderContext();

    mesh::destroy(context, &mesh_);

    render::shaderDestroy(context, &vertexShader_);
    render::shaderDestroy(context, &fragmentShader_);

    render::pipelineLayoutDestroy(context, &pipelineLayout_);
    render::graphicsPipelineDestroy(context, &pipeline_);
    render::descriptorSetLayoutDestroy(context, &descriptorSetLayout_);
    render::descriptorSetDestroy(context, &descriptorSet_);    
    render::gpuBufferDestroy(context, nullptr, &globalUnifomBuffer_);
    render::gpuBufferDestroy(context, nullptr, &particleBuffer_);

    render::shaderDestroy(context, &computeShader_);
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

    //Update uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferUpdate(context, (void*)&matrices, 0, sizeof(matrices), &globalUnifomBuffer_);

    //Render frame
    render::presentFrame(&context);

    render::commandBufferSubmit(context, computeCommandBuffer_);
    vkQueueWaitIdle(context.computeQueue_.handle_);
  }

  void onResize(u32 width, u32 height)
  {
    buildCommandBuffers();
    projectionTx_ = perspectiveProjectionMatrix(1.5f, width / (float)height, 1.0f, 1000.0f);
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
        camera_.Move(-1.0f);
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        camera_.Move(1.0f);
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
    }
  }

  void buildCommandBuffers()
  {
    render::context_t& context = getRenderContext();

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.2f, 0.3f, 0.4f, 1.0f } };

    clearValues[1].depthStencil = { 1.0f,0 };
    const VkCommandBuffer* commandBuffers;
    uint32_t count = render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      render::beginPresentationCommandBuffer(context, i, clearValues);
      bkk::render::graphicsPipelineBind(commandBuffers[i], pipeline_);
      bkk::render::descriptorSetBindForGraphics(commandBuffers[i], pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::drawInstanced(commandBuffers[i], particleCount_, nullptr, 0u, mesh_);
      render::endPresentationCommandBuffer(context, i);
    }
  }

  void createComputePipeline()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t binding = { render::descriptor_t::type::STORAGE_BUFFER, 0, render::descriptor_t::stage::COMPUTE };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &computeDescriptorSetLayout_);

    //Create pipeline layout
    render::push_constant_range_t pushConstantsRange = { VK_SHADER_STAGE_COMPUTE_BIT , sizeof(u32), 0u };
    render::pipelineLayoutCreate(context, &computeDescriptorSetLayout_, 1u, &pushConstantsRange, 1u, &computePipelineLayout_);

    //Create descriptor set
    render::descriptor_t descriptor = render::getDescriptor(particleBuffer_);
    render::descriptorSetCreate(context, descriptorPool_, computeDescriptorSetLayout_, &descriptor, &computeDescriptorSet_);

    //Create pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::COMPUTE_SHADER, gComputeShader, &computeShader_);
    render::computePipelineCreate(context, computePipelineLayout_, computeShader_, &computePipeline_);
  }

  void buildComputeCommandBuffer()
  {
    render::context_t& context = getRenderContext();

    //Build compute command buffer
    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, &computeCommandBuffer_);
    render::commandBufferBegin(context, computeCommandBuffer_);
    bkk::render::computePipelineBind(computeCommandBuffer_.handle_, computePipeline_);
    bkk::render::descriptorSetBindForCompute(computeCommandBuffer_.handle_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);
    bkk::render::pushConstants(computeCommandBuffer_.handle_, computePipelineLayout_, 0u, &particleCount_ );
    vkCmdDispatch(computeCommandBuffer_.handle_, u32( ( particleCount_ / 64.0 ) + 0.99f), 1, 1);
    render::commandBufferEnd(computeCommandBuffer_);
  }

private:

  u32 particleCount_;
  render::gpu_buffer_t globalUnifomBuffer_;

  mesh::mesh_t mesh_;
  render::gpu_buffer_t particleBuffer_;
  render::vertex_format_t vertexFormat_;

  render::pipeline_layout_t pipelineLayout_;
  render::descriptor_set_layout_t descriptorSetLayout_;

  render::descriptor_pool_t descriptorPool_;
  render::descriptor_set_t descriptorSet_;

  render::graphics_pipeline_t pipeline_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;

  camera::orbiting_camera_t camera_;
  maths::mat4 projectionTx_;
  maths::mat4 modelTx_;

  
  render::pipeline_layout_t computePipelineLayout_;
  render::descriptor_set_layout_t computeDescriptorSetLayout_;
  render::descriptor_set_t computeDescriptorSet_;
  render::compute_pipeline_t computePipeline_;
  render::command_buffer_t computeCommandBuffer_;
  render::shader_t computeShader_;
};

//Entry point
int main()
{
  particles_sample_t sample;
  sample.loop();
  return 0;
}
