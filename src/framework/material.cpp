/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/maths.h"
#include "core/string-utils.h"

#include "framework/material.h"
#include "framework/renderer.h"

using namespace bkk::framework;
using namespace bkk::core;

material_t::material_t()
:shader_(core::NULL_HANDLE),
 renderer_(nullptr)
{
}

material_t::material_t(shader_handle_t shaderHandle, renderer_t* renderer)
:shader_(shaderHandle),
 renderer_(renderer)
{
  shader_t* shader = renderer->getShader(shaderHandle);
  if (shader)
  {
    render::context_t& context = renderer->getContext();
    const std::vector<buffer_desc_t>& bufferDesc = shader->getBufferDescriptions();    
    const std::vector<texture_desc_t>& textureDesc = shader->getTextureDescriptions();
    descriptors_.resize(bufferDesc.size() + textureDesc.size());

    uint32_t passCount = shader->getPassCount();
    descriptorSet_.resize(passCount);
    updateDescriptorSet_.resize(passCount);
    for (uint32_t i = 0; i < passCount; ++i)
    {
      descriptorSet_[i] = {};
      updateDescriptorSet_[i] = true;
    }

    for (uint32_t i(0); i < bufferDesc.size(); ++i)
    {
      if (bufferDesc[i].shared == false)
      {
        uint8_t* data = new uint8_t[bufferDesc[i].size];
        memset(data, 0, bufferDesc[i].size);
        uniformData_.push_back(data);
        uniformDataSize_.push_back(bufferDesc[i].size);

        render::gpu_buffer_t ubo = {};
        render::gpuBufferCreate(context,
                                render::gpu_buffer_t::usage_e::UNIFORM_BUFFER,
                                (void*)data, bufferDesc[i].size,
                                nullptr, &ubo);

        uniformBuffers_.push_back(ubo);
        descriptors_[bufferDesc[i].binding] = render::getDescriptor(ubo);
        uniformBufferUpdate_.push_back(false);
      }
    }
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
  
  for (int i = 0; i < descriptorSet_.size(); ++i)
  {
    if (descriptorSet_[i].handle != VK_NULL_HANDLE)
    {
      render::descriptorSetDestroy(context, &descriptorSet_[i]);
    }
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

bool material_t::setProperty(const char* property, uint32_t value)
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
    if (bufferDesc[i].shared == false )
    {
      if (bufferDesc[i].name == tokens[0] )
      {
        for (int j(0); j < bufferDesc[i].fields.size(); ++j)
        {
          if (bufferDesc[i].fields[j].name == tokens[1])
          {
            ptr = uniformData_[bufferCount] + bufferDesc[i].fields[j].byteOffset;
            size = bufferDesc[i].fields[j].size;
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
  for (uint32_t i(0); i<bufferDesc.size(); ++i)
  {
    if (bufferDesc[i].shared == true && bufferDesc[i].name.compare(property) == 0)
      bindPoint = bufferDesc[i].binding;
  }

  if (bindPoint < 0) return false;

  descriptors_[bindPoint] = render::getDescriptor(buffer);

  for (int i = 0; i < descriptorSet_.size(); ++i)
  {
    if (descriptorSet_[i].handle != VK_NULL_HANDLE)
    {
      descriptorSet_[i].descriptors[bindPoint] = render::getDescriptor(buffer);
      updateDescriptorSet_[i] = true;
    }
  }

  return true;
}

bool material_t::setTexture(const char* property, render_target_handle_t randerTarget)
{
  render_target_t* targetPtr = renderer_->getRenderTarget(randerTarget);
  if (targetPtr != nullptr)
  {
    return setTexture(property, targetPtr->getColorBuffer());
  }

  return false;
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
    if (textureDesc[i].name.compare(property) == 0)
    {
      bindPoint = textureDesc[i].binding;
      break;
    }
  }

  if (bindPoint < 0) return false;

  descriptors_[bindPoint] = render::getDescriptor(texture);
  for (int i = 0; i < descriptorSet_.size(); ++i)
  {
    if (descriptorSet_[i].handle != VK_NULL_HANDLE)
    {
      descriptorSet_[i].descriptors[bindPoint] = render::getDescriptor(texture);
      updateDescriptorSet_[i] = true;
    }
  }

  return true;
}

render::descriptor_set_t material_t::getDescriptorSet(uint32_t pass)
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
      render::gpuBufferUpdate(context, uniformData_[i], 0u, uniformDataSize_[i], &uniformBuffers_[i]);
      uniformBufferUpdate_[i] = false;
    }
  }
    
  if (updateDescriptorSet_[pass])
  {
    if (descriptorSet_[pass].handle == VK_NULL_HANDLE)
    {
      render::descriptor_t* descriptorsPtr = descriptors_.empty() ? nullptr : &descriptors_[0];
      render::descriptorSetCreate(context, renderer_->getDescriptorPool(), shader->getDescriptorSetLayout(), descriptorsPtr, &descriptorSet_[pass]);
    }
    else
    {
      render::descriptorSetUpdate(context, shader->getDescriptorSetLayout(), &descriptorSet_[pass]);
    }

    updateDescriptorSet_[pass] = false;
  }

  return descriptorSet_[pass];

}
render::descriptor_set_t material_t::getDescriptorSet(const char* pass)
{
  shader_t* shader = renderer_->getShader(shader_);
  if (!shader)
    return core::render::descriptor_set_t();

  return getDescriptorSet( shader->getPassIndexFromName(pass) );
}