/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/actor.h"
#include "core/render.h"

#include "framework/renderer.h"

using namespace bkk::framework;
using namespace bkk::core;

actor_t::actor_t()
  :name_(),
  mesh_(core::BKK_NULL_HANDLE),
  transform_(core::BKK_NULL_HANDLE),
  material_(core::BKK_NULL_HANDLE)
{
}

actor_t::actor_t(const char* name, mesh_bkk_handle_t mesh, transform_bkk_handle_t transform, material_bkk_handle_t material, uint32_t instanceCount, renderer_t* renderer)
:name_(name), 
 mesh_(mesh), 
 transform_(transform), 
 material_(material),
 instanceCount_(instanceCount)
{
  core::render::context_t& context = renderer->getContext();

  core::render::gpuBufferCreate(context,
    core::render::gpu_buffer_t::UNIFORM_BUFFER,
    nullptr, sizeof(core::maths::mat4),
    nullptr, &uniformBuffer_);

  render::descriptor_t descriptor = render::getDescriptor(uniformBuffer_);
  render::descriptorSetCreate(context, 
                              renderer->getDescriptorPool(), renderer->getObjectDescriptorSetLayout(), 
                              &descriptor, &descriptorSet_);
}

void actor_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();
  render::descriptorSetDestroy(context, &descriptorSet_);
  render::gpuBufferDestroy(context, nullptr, &uniformBuffer_);
}