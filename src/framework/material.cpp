
#include "core/maths.h"
#include "core/string-utils.h"

#include "framework/material.h"
#include "framework/renderer.h"


using namespace bkk::framework;
using namespace bkk::core;

static render::gpu_buffer_t createUniformBufferFromDesc(buffer_desc_t bufferDesc)
{
  
}


material_t::material_t()
:shader_(core::NULL_HANDLE),
 renderer_(nullptr),
 descriptorSet_(),
 updateDescriptorSet_(true)
{
}

material_t::material_t(shader_handle_t shaderHandle, renderer_t* renderer)
:shader_(shaderHandle),
 renderer_(renderer),
 descriptorSet_(),
 updateDescriptorSet_(true)
{
  shader_t* shader = renderer->getShader(shaderHandle);
  if (shader)
  {
    render::context_t& context = renderer->getContext();
    const std::vector<buffer_desc_t>& bufferDesc = shader->getBufferDescriptions();    
    const std::vector<texture_desc_t>& textureDesc = shader->getTextureDescriptions();
    descriptors_.resize(bufferDesc.size() + textureDesc.size());

    for (uint32_t i(0); i < bufferDesc.size(); ++i)
    {
      if (bufferDesc[i].shared_ == false)
      {
        uint8_t* data = new uint8_t[bufferDesc[i].size_];
        memset(data, 0, bufferDesc[i].size_);
        uniformData_.push_back(data);

        render::gpu_buffer_t ubo = {};
        render::gpuBufferCreate(context,
                                render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                                (void*)data, bufferDesc[i].size_,
                                nullptr, &ubo);

        uniformBuffers_.push_back(ubo);
        descriptors_[bufferDesc[i].binding_] = render::getDescriptor(ubo);
        uniformBufferUpdate_.push_back(false);
      }
      

    }


      //Descriptor set
      //core::render::descriptor_t descriptor = core::render::getDescriptor(camera->uniformBuffer_);
      //core::render::descriptorSetCreate(context_, descriptorPool_, globalsDescriptorSetLayout_, &descriptor, &camera->descriptorSet_);
    
  }
}

void material_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();
  for (uint32_t i(0); i < uniformBuffers_.size(); ++i)
  {
    render::gpuBufferDestroy(context, nullptr, &uniformBuffers_[i]);
    delete[] uniformData_[i];
  }

  if (descriptorSet_.handle_ != VK_NULL_HANDLE)
  {
    render::descriptorSetDestroy(context, &descriptorSet_);
  }
}

render::graphics_pipeline_t material_t::getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer)
{
  shader_t* shader = renderer->getShader(shader_);
  if (shader)
  {
    return shader->getPipeline(name, framebuffer, renderer);
  }

  core::render::graphics_pipeline_t nullPipeline = {};
  return nullPipeline;
}

bool material_t::setProperty(const char* property, float value)
{ 
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, const maths::vec2& value) 
{
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, const maths::vec3& value) 
{
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, const maths::vec4& value) 
{
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, const maths::mat3& value) 
{
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, const maths::mat4& value) 
{
  return setProperty(property, (void*)&value);
}

bool material_t::setProperty(const char* property, void* value)
{
  shader_t* shader = renderer_->getShader(shader_);
  if (!shader) return false;

  char delimiter = '.';
  std::vector<std::string> tokens;
  splitString(property, &delimiter, 1, &tokens);
  
  //property name should have the buffer name and the property name
  if (tokens.size() < 2) return false;

  //Find buffer
  const std::vector<buffer_desc_t>& bufferDesc = shader->getBufferDescriptions();
  
  uint32_t bufferCount = 0u;
  uint8_t* ptr = nullptr;
  uint32_t size = 0u;
  for (uint32_t i(0); bufferDesc.size(); ++i)
  {
    if (bufferDesc[i].shared_ == false )
    {
      if (bufferDesc[i].name_ == tokens[0] )
      {
        for (int j(0); j < bufferDesc[i].fields_.size(); ++j)
        {
          if (bufferDesc[i].fields_[j].name_ == tokens[1])
          {
            ptr = uniformData_[bufferCount] + bufferDesc[i].fields_[j].byteOffset_;
            size = bufferDesc[i].fields_[j].size_;
            uniformBufferUpdate_[bufferCount] = true;
            break;
          }
        }
        break;
      }

      bufferCount++;
    }
  }

  if (!ptr)  return false;

  memcpy(ptr, value, size);

  return true;
}

bool material_t::setBuffer(const char* property, render::gpu_buffer_t buffer)
{
  shader_t* shader = renderer_->getShader(shader_);
  if (!shader) return false;

  //Find buffer
  const std::vector<buffer_desc_t>& bufferDesc = shader->getBufferDescriptions();
  int32_t bindPoint = -1;
  for (uint32_t i(0); bufferDesc.size(); ++i)
  {
    if (bufferDesc[i].shared_ == true && bufferDesc[i].name_.compare(property) == 0)
      bindPoint = bufferDesc[i].binding_;
  }

  if (bindPoint < 0) return false;

  descriptors_[bindPoint] = render::getDescriptor(buffer);
  if (descriptorSet_.handle_ != VK_NULL_HANDLE)
  {
    descriptorSet_.descriptors_[bindPoint] = render::getDescriptor(buffer);
    updateDescriptorSet_ = true;
  }

  return true;
}

bool material_t::setTexture(const char* property, render::texture_t texture)
{
  shader_t* shader = renderer_->getShader(shader_);
  if (!shader) return false;

  //Find texture
  const std::vector<texture_desc_t>& textureDesc = shader->getTextureDescriptions();
  int32_t bindPoint = -1;
  for (uint32_t i(0); i<textureDesc.size(); ++i)
  {
    if (textureDesc[i].name_.compare(property) == 0)
      bindPoint = textureDesc[i].binding_;
  }

  if (bindPoint < 0) return false;

  descriptors_[bindPoint] = render::getDescriptor(texture);
  if (descriptorSet_.handle_ != VK_NULL_HANDLE)
  {
    descriptorSet_.descriptors_[bindPoint] = render::getDescriptor(texture);
    updateDescriptorSet_ = true;
  }

  return true;
}

render::descriptor_set_t material_t::getDescriptorSet()
{
  render::context_t& context = renderer_->getContext();

  shader_t* shader = renderer_->getShader(shader_);
  if (!shader)
    return core::render::descriptor_set_t();

  //Update owned uniform buffer if needed
  for (uint32_t i(0); i < uniformBufferUpdate_.size(); ++i)
  {
    if (uniformBufferUpdate_[i])
    {
      render::gpuBufferUpdate(context, uniformData_[i], 0u, uniformBuffers_[i].memory_.size_, &uniformBuffers_[i]);
      uniformBufferUpdate_[i] = false;
    }
  }

  if ( updateDescriptorSet_ )
  {
    if (descriptorSet_.handle_ == VK_NULL_HANDLE)
    {
      render::descriptor_t* descriptorsPtr = descriptors_.empty() ? nullptr : &descriptors_[0];
      render::descriptorSetCreate(context, renderer_->getDescriptorPool(), shader->getDescriptorSetLayout(), descriptorsPtr, &descriptorSet_);
    }
    else
    {
    }

    updateDescriptorSet_ = true;
  }

  return descriptorSet_;
}