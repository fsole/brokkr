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
#include "maths.h"
#include "../utility.h"
#include "timer.h"

using namespace bkk;
using namespace maths;
using namespace sample_utils;

static const char* gVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec3 aNormal;\n \
  layout(location = 2) in vec2 aTexCoord;\n \
  layout(location = 3) in vec4 aBonesWeight;\n \
  layout(location = 4) in vec4 aBonesId;\n \
  layout(binding = 1) uniform UNIFORMS\n \
  {\n \
    mat4 modelView;\n \
    mat4 modelViewProjection;\n \
  }uniforms;\n \
  layout(binding = 2) uniform BONESTX\n \
  {\n \
    mat4 bones[64];\n \
  }bonesTx;\n \
  out vec3 normalViewSpace;\n \
  out vec3 lightViewSpace;\n \
  void main(void)\n \
  {\n \
    mat4 transform = bonesTx.bones[int(aBonesId[0])] * aBonesWeight[0] +  \n \
    bonesTx.bones[int(aBonesId[1])] * aBonesWeight[1] +					  \n \
    bonesTx.bones[int(aBonesId[2])] * aBonesWeight[2] +					  \n \
    bonesTx.bones[int(aBonesId[3])] * aBonesWeight[3];                    \n \
    gl_Position = uniforms.modelViewProjection * transform * vec4(aPosition,1.0); \n \
    mat4 normalTransform = mat4(inverse(transpose(uniforms.modelView * transform)));\n \
    normalViewSpace = normalize((normalTransform * vec4(aNormal,0.0)).xyz);\n \
    vec3 lightPositionModelSpace = vec3(-0.5,0.5,1.0);\n \
    lightViewSpace = normalize((uniforms.modelView * vec4(normalize(lightPositionModelSpace),0.0)).xyz);\n \
  }\n"
};

static const char* gFragmentShaderSource = {
  "#version 440 core\n \
  in vec3 normalViewSpace;\n \
  in vec3 lightViewSpace;\n \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    float diffuse = max(dot(normalize(lightViewSpace), normalize(normalViewSpace)), 0.0);\n \
    color = vec4(vec3(diffuse), 1.0);\n \
  }\n"
};


class skinning_sample_t : public application_t
{
public:
  skinning_sample_t(const char* title)
  :application_t( title, 600u, 600u, 3u ),
   time_(0.0f)
  {
    createUniformBuffer();
    createGeometry();
    createPipeline();
    buildCommandBuffers();
  }

  void onQuit()
  {
    //Destroy all resources
    render::context_t& context = getRenderContext();

    mesh::destroy(context, &mesh_);
    mesh::animatorDestroy(context, &animator_);
    render::gpuBufferDestroy(context, nullptr, &ubo_);

    render::shaderDestroy(context, &vertexShader_);
    render::shaderDestroy(context, &fragmentShader_);

    render::graphicsPipelineDestroy(context, &pipeline_);
    render::descriptorSetDestroy(context, &descriptorSet_);
    render::descriptorSetLayoutDestroy(context, &descriptorSetLayout_);
    render::descriptorPoolDestroy(context, &descriptorPool_);
    render::pipelineLayoutDestroy(context, &pipelineLayout_);
  }
  
  void render()
  {
    time_ += getTimeDelta() * 0.001f;
    render::context_t& context = getRenderContext();
    mesh::animatorUpdate(context, time_, &animator_);
    render::presentFrame(&context);
  }
  
  void onResize(u32 width, u32 height) 
  {
    buildCommandBuffers();
    projectionTx_ = computePerspectiveProjectionMatrix(1.5f, width / (float)height, 1.0f, 1000.0f);
    updateUniformBuffer();
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
          updateUniformBuffer();
          break;
        }
        case window::key_e::KEY_DOWN:
        case 's':
        {
          camera_.Move(1.0f);
          updateUniformBuffer();
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
      updateUniformBuffer();
    }
  }

  void createUniformBuffer()
  {
    camera_.offset_ = 35.0f;
    camera_.angle_ = vec2(0.8f, 0.0f);
    camera_.Update();

    uvec2 windowSize = getWindowSize();
    projectionTx_ = computePerspectiveProjectionMatrix(1.5f, windowSize.x / (float)windowSize.y, 1.0f, 1000.0f);
    modelTx_ = computeTransform(VEC3_ZERO, vec3(0.01f, 0.01f, 0.01f), quaternionFromAxisAngle(vec3(1.0f, 0.0f, 0.0f), degreeToRadian(90.0f)));

    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;

    //Create uniform buffer
    render::gpuBufferCreate(getRenderContext(), render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&matrices, 2 * sizeof(mat4),
      nullptr, &ubo_);
  }

  void updateUniformBuffer()
  {
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;

    render::gpuBufferUpdate(getRenderContext(), (void*)&matrices, 0, 2 * sizeof(mat4), &ubo_);
  }

  void createGeometry()
  {
    render::context_t& context = getRenderContext();
    mesh::createFromFile(context, "../resources/goblin.dae", mesh::EXPORT_ALL, nullptr, 0u, &mesh_);
    mesh::animatorCreate(context, mesh_, 0u, 5.0f, &animator_);
  }

  void createPipeline()
  {
    //Create descriptor layout  
    render::descriptor_binding_t bindings[2] = {
      render::descriptor_binding_t{ render::descriptor_t::type::UNIFORM_BUFFER, 1, render::descriptor_t::stage::VERTEX },
      render::descriptor_binding_t{ render::descriptor_t::type::UNIFORM_BUFFER, 2, render::descriptor_t::stage::VERTEX }
    };

    render::context_t& context = getRenderContext();

    render::descriptorSetLayoutCreate(context, bindings, 2u, &descriptorSetLayout_);

    //Create pipeline layout
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, &pipelineLayout_);

    //Create descriptor pool
    render::descriptorPoolCreate(context, 1u, 0u, 2u, 0u, 0u, &descriptorPool_);

    //Create descriptor set
    render::descriptor_t descriptors[2] = { render::getDescriptor(ubo_), render::getDescriptor(animator_.buffer_) };
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, descriptors, &descriptorSet_);

    //Load shaders
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);

    //Create pipeline
    bkk::render::graphics_pipeline_t::description_t pipelineDesc;
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
    pipelineDesc.depthTestEnabled_ = true;
    pipelineDesc.depthWriteEnabled_ = true;
    pipelineDesc.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader_ = vertexShader_;
    pipelineDesc.fragmentShader_ = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh_.vertexFormat_, pipelineLayout_, pipelineDesc, &pipeline_);
  }

  void buildCommandBuffers()
  {
    render::context_t& context = getRenderContext();

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.0f, 0.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f,0 };    
    for (unsigned i(0); i<3; ++i)
    {
      VkCommandBuffer cmdBuffer = render::beginPresentationCommandBuffer(context, i, clearValues);
      bkk::render::graphicsPipelineBind(cmdBuffer, pipeline_);
      bkk::render::descriptorSetBindForGraphics(cmdBuffer, pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::draw(cmdBuffer, mesh_);
      render::endPresentationCommandBuffer(context, i);
    }
  }

private:
  
  mesh::mesh_t mesh_;
  mesh::skeletal_animator_t animator_;

  render::gpu_buffer_t ubo_;
  render::pipeline_layout_t pipelineLayout_;
  render::descriptor_pool_t descriptorPool_;
  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_t descriptorSet_;
  render::graphics_pipeline_t pipeline_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;

  maths::mat4 modelTx_;
  maths::mat4 projectionTx_;
  float time_;
  orbiting_camera_t camera_;
};


//Entry point
int main()
{
  skinning_sample_t sample("Skinning");
  sample.loop();
  return 0;
}
