
#include "core/maths.h"
#include "core/mesh.h"
#include "core/render.h"
#include "core/window.h"

#include "framework/renderer.h"
#include "framework/gui.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::framework;

static const char* gTextureBlitVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;

  layout(location = 0) out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition,1.0);
    uv = aTexCoord;
  }
)";

static const char* gTextureBlitFragmentShaderSource = R"(
  #version 440 core

  layout(location = 0)in vec2 uv;
  layout(location = 0) out vec4 color;
  layout (binding = 0) uniform sampler2D uTexture;

  void main(void)
  {
    color = texture(uTexture, uv);
  }
)";

renderer_t::renderer_t()
:context_(),
 backBuffer_(NULL_HANDLE),
 activeCamera_(NULL_HANDLE)
{}

renderer_t::~renderer_t()
{
  if (context_.instance_ != VK_NULL_HANDLE)
  {
    actor_t* actors;
    uint32_t count = actors_.getData(&actors);
    for (uint32_t i = 0; i < count; ++i)
      actors[i].destroy(this);

    camera_t* cameras;
    count = cameras_.getData(&cameras);
    for (uint32_t i = 0; i < count; ++i)
      cameras[i].destroy(this);

    mesh::mesh_t* meshes;
    count = meshes_.getData(&meshes);
    for (uint32_t i = 0; i < count; ++i)
      mesh::destroy(context_, &meshes[i]);

    material_t* materials;
    count = materials_.getData(&materials);
    for (uint32_t i = 0; i < count; ++i)
      materials[i].destroy(this);

    render_target_t* renderTargets;
    count = renderTargets_.getData(&renderTargets);
    for (uint32_t i = 0; i < count; ++i)
      renderTargets[i].destroy(this);

    frame_buffer_t* framebuffers;
    count = framebuffers_.getData(&framebuffers);
    for (uint32_t i = 0; i < count; ++i)
      framebuffers[i].destroy(this);

    shader_t* shaders;
    count = shaders_.getData(&shaders);
    for (uint32_t i = 0; i < count; ++i)
      shaders[i].destroy(this);

    for (uint32_t i(0); i < releasedCommandBuffers_.size(); ++i)
      releasedCommandBuffers_[i].cleanup();

    if (backBuffer_ != NULL_HANDLE )
    {
      render::descriptorSetLayoutDestroy(context_, &textureBlitDescriptorSetLayout_);
      render::descriptorSetDestroy(context_, &presentationDescriptorSet_);
      render::pipelineLayoutDestroy(context_, &textureBlitPipelineLayout_);
      render::graphicsPipelineDestroy(context_, &presentationPipeline_);
      render::shaderDestroy(context_, &textureBlitVertexShader_);
      render::shaderDestroy(context_, &textureBlitFragmentShader_);
      render::semaphoreDestroy(context_, renderComplete_);
      mesh::destroy(context_, &fullScreenQuad_);
    }

    render::descriptorSetLayoutDestroy(context_, &globalsDescriptorSetLayout_);
    render::descriptorSetLayoutDestroy(context_, &objectDescriptorSetLayout_);
    render::descriptorPoolDestroy(context_, &globalDescriptorPool_);
    render::contextDestroy(&context_);
  }
}

void renderer_t::initialize(const char* title, uint32_t imageCount, const window::window_t& window)
{
  render::contextCreate(title, "", window, imageCount, &context_);

  render::descriptor_binding_t binding = { render::descriptor_t::type::UNIFORM_BUFFER, 0, render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT };
  render::descriptorSetLayoutCreate(context_, &binding, 1u, &globalsDescriptorSetLayout_);
  render::descriptorSetLayoutCreate(context_, &binding, 1u, &objectDescriptorSetLayout_);

  render::descriptorPoolCreate(context_, 1000u,
    render::combined_image_sampler_count(1000u),
    render::uniform_buffer_count(1000u),
    render::storage_buffer_count(1000u),
    render::storage_image_count(1000u),
    &globalDescriptorPool_);

  
  shader_handle_t shader = shaderCreate("../../shaders/textureBlit.shader");
  textureBlit_ = materialCreate(shader);
  mesh_handle_t quad = addMesh( mesh::fullScreenQuad(context_) );
  rootActor_ = actorCreate("Root", quad, textureBlit_);
}

render::context_t& renderer_t::getContext()
{
  return context_;
}

shader_handle_t renderer_t::shaderCreate(const char* file)
{
  return shaders_.add(shader_t(file, this));
}

shader_t* renderer_t::getShader(shader_handle_t handle)
{
  return shaders_.get(handle);
}


material_handle_t renderer_t::materialCreate(shader_handle_t shader)
{
  return materials_.add(material_t(shader, this));
}

material_t* renderer_t::getMaterial(material_handle_t handle)
{
  return materials_.get(handle);
}


render_target_handle_t renderer_t::renderTargetCreate(uint32_t width, uint32_t height,
  VkFormat format,
  bool depthBuffer)
{
  return renderTargets_.add(
    render_target_t(width, height, format, depthBuffer, this));
}

render_target_t* renderer_t::getRenderTarget(render_target_handle_t handle)
{
  return renderTargets_.get(handle);
}

frame_buffer_handle_t renderer_t::frameBufferCreate(render_target_handle_t* renderTargets, uint32_t targetCount,
  VkImageLayout* initialLayouts, VkImageLayout* finalLayouts)
{
  return framebuffers_.add(
    frame_buffer_t( renderTargets, targetCount, initialLayouts, finalLayouts, this) );
}

frame_buffer_t* renderer_t::getFrameBuffer(frame_buffer_handle_t handle)
{
  return framebuffers_.get(handle);
}


mesh_handle_t renderer_t::addMesh(mesh::mesh_t& mesh)
{
  return meshes_.add(mesh);
}

mesh::mesh_t* renderer_t::getMesh(mesh_handle_t handle)
{
  return meshes_.get(handle);
}

actor_handle_t renderer_t::actorCreate(const char* name, mesh_handle_t mesh, material_handle_t material, maths::mat4 transform)
{
  bkk::core::handle_t transformHandle = transformManager_.createTransform(transform);

  return actors_.add(
    actor_t(name, mesh, transformHandle, material, this) );
}

actor_t* renderer_t::getActor(actor_handle_t handle)
{
  return actors_.get(handle);
}

void renderer_t::actorSetParent(actor_handle_t actor, actor_handle_t parent)
{
  transformManager_.setParent(actors_.get(actor)->getTransform(), actors_.get(parent)->getTransform());
}

void renderer_t::actorSetTransform(actor_handle_t actor, const maths::mat4& newTransform)
{
  transformManager_.setTransform(actors_.get(actor)->getTransform(), newTransform);
}

camera_handle_t renderer_t::addCamera(camera_t& camera)
{
  return cameras_.add(camera);
}

camera_t* renderer_t::getCamera(camera_handle_t handle)
{
  return cameras_.get(handle);
}

camera_t* renderer_t::getActiveCamera()
{
  return cameras_.get(activeCamera_);
}

bool renderer_t::setupCamera(camera_handle_t handle)
{
  camera_t* camera = cameras_.get(handle);
  if (!camera)
    return false;

  camera->update(this);

  ////Culling
  actor_t* allActors;
  uint32_t actorCount = actors_.getData(&allActors);
  camera->cull(allActors, actorCount);

  activeCamera_ = handle;

  return true;
}

int renderer_t::getVisibleActors(camera_handle_t cameraHandle, actor_t** actors)
{
  camera_t* camera = cameras_.get(cameraHandle);
  if (!camera)
    return 0;

  *actors = camera->visibleActors_;
  return camera->visibleActorsCount_;
}

void renderer_t::presentFrame()
{
  render::presentFrame(&context_, &renderComplete_, 1u);

  for (uint32_t i(0); i < releasedCommandBuffers_.size(); ++i)
    releasedCommandBuffers_[i].cleanup();

  releasedCommandBuffers_.clear();
}

void renderer_t::update()
{
  //Update transform manager and uniform buffer
  transformManager_.update();

  actor_t* actors;
  uint32_t actorCount = actors_.getData(&actors);
  for (u32 i(0); i < actorCount; ++i)
  {
    render::gpuBufferUpdate(context_, transformManager_.getWorldMatrix(actors[i].transform_), 
                                  0, sizeof(maths::mat4), &actors[i].uniformBuffer_);
  }

  buildPresentationCommandBuffers();
}

void renderer_t::createTextureBlitResources()
{
  render_target_handle_t colorBufferHandle = renderTargetCreate(context_.swapChain_.imageWidth_, context_.swapChain_.imageHeight_, VK_FORMAT_R32G32B32A32_SFLOAT, false);
  backBuffer_ = frameBufferCreate(&colorBufferHandle, 1u);

  fullScreenQuad_ = mesh::fullScreenQuad(context_);
  render::descriptor_binding_t binding = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage::FRAGMENT };
  render::descriptorSetLayoutCreate(context_, &binding, 1u, &textureBlitDescriptorSetLayout_);

  render::descriptor_t descriptor = render::getDescriptor(renderTargets_.get(colorBufferHandle)->getColorBuffer());
  render::descriptorSetCreate(context_, globalDescriptorPool_, textureBlitDescriptorSetLayout_, &descriptor, &presentationDescriptorSet_);

  render::pipelineLayoutCreate(context_, &textureBlitDescriptorSetLayout_, 1u, nullptr, 0u, &textureBlitPipelineLayout_);
  render::shaderCreateFromGLSLSource(context_, render::shader_t::VERTEX_SHADER, gTextureBlitVertexShaderSource, &textureBlitVertexShader_);
  render::shaderCreateFromGLSLSource(context_, render::shader_t::FRAGMENT_SHADER, gTextureBlitFragmentShaderSource, &textureBlitFragmentShader_);

  //Create pipeline
  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context_.swapChain_.imageWidth_, (float)context_.swapChain_.imageHeight_, 0.0f, 1.0f };
  pipelineDesc.scissorRect_ = { { 0,0 },{ context_.swapChain_.imageWidth_,context_.swapChain_.imageHeight_ } };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = false;
  pipelineDesc.depthWriteEnabled_ = false;
  pipelineDesc.vertexShader_ = textureBlitVertexShader_;
  pipelineDesc.fragmentShader_ = textureBlitFragmentShader_;
  render::graphicsPipelineCreate(context_, context_.swapChain_.renderPass_, 0u, fullScreenQuad_.vertexFormat_, textureBlitPipelineLayout_, pipelineDesc, &presentationPipeline_);

  renderComplete_ = render::semaphoreCreate(context_);
}
void renderer_t::buildPresentationCommandBuffers()
{
  if (backBuffer_ == NULL_HANDLE)
  {
    createTextureBlitResources();
  }

  const render::command_buffer_t* commandBuffers;
  uint32_t count = render::getPresentationCommandBuffers(context_, &commandBuffers);
  for (uint32_t i(0); i<count; ++i)
  {
    render::beginPresentationCommandBuffer(context_, i, nullptr);
    render::graphicsPipelineBind(commandBuffers[i], presentationPipeline_);
    render::descriptorSetBind(commandBuffers[i], textureBlitPipelineLayout_, 0, &presentationDescriptorSet_, 1u);
    mesh::draw(commandBuffers[i], fullScreenQuad_);

    framework::gui::draw(context_, commandBuffers[i]);
    render::endPresentationCommandBuffer(context_, i);
  }
}

frame_buffer_handle_t renderer_t::getBackBuffer()
{
  return backBuffer_;
}

VkSemaphore* renderer_t::getRenderCompleteSemaphore()
{
  return &renderComplete_;
}

render::descriptor_set_layout_t renderer_t::getGlobalsDescriptorSetLayout()
{
  return globalsDescriptorSetLayout_;
}

render::descriptor_set_layout_t renderer_t::getObjectDescriptorSetLayout()
{
  return objectDescriptorSetLayout_;
}

render::descriptor_pool_t renderer_t::getDescriptorPool() {
  return globalDescriptorPool_;
}

void renderer_t::releaseCommandBuffer(const command_buffer_t* cmdBuffer)
{
  releasedCommandBuffers_.push_back(*cmdBuffer);
}