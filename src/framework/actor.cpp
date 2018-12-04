
#include "framework/actor.h"
#include "core/render.h"

#include "framework/renderer.h"

using namespace bkk::framework;
using namespace bkk::core;

actor_t::actor_t()
  :name_(),
  mesh_(core::NULL_HANDLE),
  transform_(core::NULL_HANDLE),
  material_(core::NULL_HANDLE)
{
}

actor_t::actor_t(const char* name, mesh_handle_t mesh, transform_handle_t transform, material_handle_t material, renderer_t* renderer)
  :name_(name), mesh_(mesh), transform_(transform), material_(material)
{
  core::render::context_t& context = renderer->getContext();

  core::render::gpuBufferCreate(context,
    core::render::gpu_buffer_t::usage::UNIFORM_BUFFER,
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

mesh_handle_t actor_t::getMesh() {
  return mesh_;
}

transform_handle_t actor_t::getTransform() {
  return transform_;
}

material_handle_t actor_t::getMaterial() {
  return material_;
}

const char* actor_t::getName() { 
  return name_.c_str(); 
}