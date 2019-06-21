/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/renderer.h"
#include "framework/compute-material.h"

using namespace bkk::framework;
using namespace bkk::core;

compute_material_t::compute_material_t()
:material_t()
{
}

compute_material_t::compute_material_t(shader_handle_t shaderHandle, renderer_t* renderer)
:material_t(shaderHandle, renderer)
{
}

void compute_material_t::dispatch(render::command_buffer_t& commandBuffer, const char* passName, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
  shader_t* computeShader = renderer_->getShader(shader_);
  if (!computeShader) return;

  uint32_t pass = computeShader->getPassIndexFromName(passName);
  return dispatch(commandBuffer, pass, groupSizeX, groupSizeY, groupSizeZ);
}

void compute_material_t::dispatch(render::command_buffer_t& commandBuffer, uint32_t pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
  render::context_t& context = renderer_->getContext();

  shader_t* computeShader = renderer_->getShader(shader_);
  if (!computeShader) return;

  render::commandBufferBegin(renderer_->getContext(), commandBuffer);

  render::compute_pipeline_t pipeline = computeShader->getComputePipeline(pass);
  if (pipeline.handle != VK_NULL_HANDLE)
  {
    render::computePipelineBind(commandBuffer, pipeline);
    render::descriptor_set_t descriptorSet = getDescriptorSet(pass);
    render::descriptorSetBind(commandBuffer, computeShader->getPipelineLayout(pass), 0, &descriptorSet, 1u);
    render::computeDispatch(commandBuffer, groupSizeX, groupSizeY, groupSizeZ);
  }
  render::commandBufferEnd(commandBuffer);
}