/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/mesh.h"
#include "core/maths.h"

#include "framework/command-buffer.h"
#include "framework/frame-buffer.h"
#include "framework/renderer.h"
#include "framework/camera.h"


using namespace bkk;
using namespace bkk::core;
using namespace bkk::framework;


command_buffer_t::command_buffer_t()
  :renderer_(nullptr),
  frameBuffer_(BKK_NULL_HANDLE),
  commandBuffer_(),
  semaphore_(),
  clearColor_(0.0f, 0.0f, 0.0f, 0.0f),
  clear_(false),
  released_(false),
  signalSemaphore_(VK_NULL_HANDLE)
{}

command_buffer_t::command_buffer_t(renderer_t* renderer, const char* name, VkSemaphore signalSemaphore, VkCommandPool pool)
:renderer_(renderer), 
 commandBuffer_(),
 commandPool_(pool), 
 semaphore_(render::semaphoreCreate(renderer->getContext())),
 frameBuffer_(renderer->getBackBuffer()),
 clearColor_(0.0f, 0.0f, 0.0f, 0.0f),
 clear_(false),
 released_(false),
 signalSemaphore_(signalSemaphore)
{
  name_ = name ? name : "";
}

void command_buffer_t::init(renderer_t* renderer, const char* name, VkSemaphore signalSemaphore, VkCommandPool pool)
{
  if (renderer_ == nullptr)
  {
    renderer_ = renderer;
    semaphore_ = render::semaphoreCreate(renderer->getContext());
    commandPool_ = pool;
    frameBuffer_ = renderer->getBackBuffer();
    signalSemaphore_ = signalSemaphore;
    name_ = name ? name : "";
  }
}

command_buffer_t::~command_buffer_t()
{
}

void command_buffer_t::setFrameBuffer(frame_buffer_handle_t framebuffer)
{
  if (framebuffer == BKK_NULL_HANDLE)
  {
    frameBuffer_ = renderer_->getBackBuffer();
  }
  else
  {
    frameBuffer_ = framebuffer;
  }
}
void command_buffer_t::setDependencies(command_buffer_t* prevCommandBuffers, uint32_t count)
{
  dependencies_.resize(count);
  for (uint32_t i(0); i < count; ++i)
    dependencies_[i] = prevCommandBuffers[i];
}

void command_buffer_t::createCommandBuffer(type_e type)
{
  if (commandBuffer_.handle != VK_NULL_HANDLE)
    return;

  std::vector<VkSemaphore> signalSemaphores(1);
  signalSemaphores[0] = semaphore_;
  if (signalSemaphore_ != VK_NULL_HANDLE )
  {
    //Rendering to back buffer
    signalSemaphores.push_back( renderer_->getRenderCompleteSemaphore() );
  }

  uint32_t dependencyCount = (uint32_t)dependencies_.size();
  std::vector<VkSemaphore> waitSemaphores(dependencyCount);
  std::vector<VkPipelineStageFlags> waitStage(dependencyCount);
  for (uint32_t i(0); i < dependencyCount; ++i)
  {
    waitSemaphores[i] = dependencies_[i].getSemaphore();
    waitStage[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
  
  core::render::command_buffer_t::type_e commandType = type == GRAPHICS ?
    core::render::command_buffer_t::GRAPHICS :
    core::render::command_buffer_t::COMPUTE;
  VkSemaphore* waitSemaphorePtr = dependencyCount == 0 ? nullptr : &waitSemaphores[0];
  VkPipelineStageFlags* waitStagePtr = dependencyCount == 0 ? nullptr : &waitStage[0];

  render::commandBufferCreate(renderer_->getContext(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, waitSemaphorePtr, waitStagePtr, 
    dependencyCount, signalSemaphores.data(), (uint32_t)signalSemaphores.size(), commandType, commandPool_, &commandBuffer_);
}

void command_buffer_t::clearRenderTargets(core::maths::vec4 color)
{
  clear_ = true;
  clearColor_ = color;
}

void command_buffer_t::beginCommandBuffer()
{  
  createCommandBuffer(GRAPHICS);

  if (commandBuffer_.handle == VK_NULL_HANDLE)
    return;

  render::context_t& context = renderer_->getContext();

  frame_buffer_t* frameBuffer = renderer_->getFrameBuffer(frameBuffer_);
  render::commandBufferBegin(context, commandBuffer_);

  VkClearValue* clearValues = nullptr;
  uint32_t clearValuesCount = 0u;
  if (clear_)
  {
    clearValuesCount = frameBuffer->getTargetCount() + 1;
    clearValues = new VkClearValue[clearValuesCount];
    for (uint32_t i(0); i < clearValuesCount - 1; ++i)
      clearValues[i].color = { { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w } };

    clearValues[clearValuesCount - 1].depthStencil = { 1.0f,0 };
  }

  render::commandBufferRenderPassBegin(context, &frameBuffer->getFrameBuffer(), &clearValues[0], clearValuesCount, commandBuffer_);

  if (!name_.empty())
    render::commandBufferDebugMarkerBegin(renderer_->getContext(), commandBuffer_, name_.c_str());

  if (clearValues)
    delete[] clearValues;
}

void command_buffer_t::endCommandBuffer()
{
  if (!name_.empty())
    render::commandBufferDebugMarkerEnd(renderer_->getContext(), commandBuffer_);

  render::commandBufferRenderPassEnd(commandBuffer_);
  render::commandBufferEnd(commandBuffer_);
}

void command_buffer_t::render(actor_t* actors, uint32_t actorCount, const char* passName)
{
  if (!renderer_) return;

  camera_t* camera = renderer_->getActiveCamera();

  beginCommandBuffer();
  
  if (commandBuffer_.handle == VK_NULL_HANDLE)
    return;

  for (uint32_t i = 0; i < actorCount; ++i)
  {
    material_t* material = renderer_->getMaterial(actors[i].getMaterialHandle());
    core::mesh::mesh_t* mesh = renderer_->getMesh(actors[i].getMeshHandle());

    if (material && mesh )
    {
      core::render::graphics_pipeline_t pipeline = material->getPipeline(passName, frameBuffer_, renderer_);
      if (pipeline.handle != VK_NULL_HANDLE)
      {
        //TODO: Order objects by material and bind pipeline and camera ubo only once for all objects
        //sharing the same material
        render::graphicsPipelineBind(commandBuffer_, pipeline );

        //Camera uniform buffer
        render::descriptorSetBind(commandBuffer_, pipeline.layout, 0, &camera->getDescriptorSet(), 1u);
      
        //Object uniform buffer
        render::descriptorSetBind(commandBuffer_, pipeline.layout, 1, &actors[i].getDescriptorSet(), 1u);

        //Material descriptor set
        render::descriptor_set_t materialDescriptorSet = material->getDescriptorSet(passName);
        render::descriptorSetBind(commandBuffer_, pipeline.layout, 2, &materialDescriptorSet, 1u);

        //Draw call
        uint32_t instanceCount = actors[i].getInstanceCount();
        if (instanceCount == 1)
        {
          core::mesh::draw(commandBuffer_, *mesh);
        }
        else
        {
          core::mesh::drawInstanced(commandBuffer_, instanceCount, nullptr, 0u, *mesh);
        }
      }
    }
  }
  
  endCommandBuffer();
}

void command_buffer_t::blit(render_target_handle_t renderTarget, material_handle_t materialHandle, const char* pass)
{
  bkk::core::render::texture_t texture = {};
  if (renderer_  && renderTarget != core::BKK_NULL_HANDLE)
  {
    texture = renderer_->getRenderTarget(renderTarget)->getColorBuffer();    
  }

  blit(texture, materialHandle, pass);
}

void command_buffer_t::blit(const bkk::core::render::texture_t& texture, material_handle_t materialHandle, const char* pass)
{
  if (!renderer_) return;

  material_t* material = nullptr;
  if (materialHandle != BKK_NULL_HANDLE)
  {
    material = renderer_->getMaterial(materialHandle);
  }
  else
  {
    material = renderer_->getTextureBlitMaterial();
  }

  if (!material) return;

  if (render::textureIsValid(texture) )
    material->setTexture("MainTexture", texture);

  material->updateDescriptorSets();
  
  camera_t* camera = renderer_->getActiveCamera();
  actor_t* actor = renderer_->getActor(renderer_->getRootActor());
  mesh::mesh_t* mesh = renderer_->getMesh(actor->getMeshHandle());

  const char* passName = "blit";
  if (pass != nullptr)
    passName = pass;

  core::render::graphics_pipeline_t pipeline = material->getPipeline(passName, frameBuffer_, renderer_);
  render::descriptor_set_t materialDescriptorSet = material->getDescriptorSet(passName);

  beginCommandBuffer();

  render::graphicsPipelineBind(commandBuffer_, pipeline);
  render::descriptorSetBind(commandBuffer_, pipeline.layout, 0, &camera->getDescriptorSet(), 1u);
  render::descriptorSetBind(commandBuffer_, pipeline.layout, 1, &actor->getDescriptorSet(), 1u);
  render::descriptorSetBind(commandBuffer_, pipeline.layout, 2, &materialDescriptorSet, 1u);

  core::mesh::draw(commandBuffer_, *mesh);

  endCommandBuffer();
}

void command_buffer_t::dispatchCompute(compute_material_handle_t computeMaterial, uint32_t pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
  if (!renderer_) return;

  compute_material_t* computeMaterialPtr = renderer_->getComputeMaterial(computeMaterial);
  if (computeMaterialPtr == nullptr)
    return;

  createCommandBuffer(COMPUTE);
  computeMaterialPtr->dispatch(commandBuffer_, pass, groupSizeX, groupSizeY, groupSizeZ);
}

void command_buffer_t::dispatchCompute(compute_material_handle_t computeMaterial, const char* pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
  if (!renderer_) return;
  
  compute_material_t* computeMaterialPtr = renderer_->getComputeMaterial(computeMaterial);
  if (computeMaterialPtr == nullptr)
    return;

  createCommandBuffer(COMPUTE);
  computeMaterialPtr->dispatch(commandBuffer_, pass, groupSizeX, groupSizeY, groupSizeZ);
}

void command_buffer_t::submit()
{
  if (renderer_ && commandBuffer_.handle != VK_NULL_HANDLE)
  {
    render::context_t& context = renderer_->getContext();
    render::commandBufferSubmit(context, commandBuffer_);
  }
}

void command_buffer_t::release()
{
  if (renderer_ && !released_)
  {    
    renderer_->releaseCommandBuffer(this);
    released_ = true;
  }
}

void command_buffer_t::submitAndRelease()
{
  if (renderer_ && commandBuffer_.handle != VK_NULL_HANDLE)
  {
    render::context_t& context = renderer_->getContext();
    render::commandBufferSubmit(context, commandBuffer_);
    renderer_->releaseCommandBuffer(this);
    released_ = true;
  }
}

void command_buffer_t::cleanup()
{
  if (renderer_ && commandBuffer_.handle != VK_NULL_HANDLE)
  {
    render::context_t& context = renderer_->getContext();
    core::render::commandBufferDestroy(context, &commandBuffer_);
    commandBuffer_ = {};
    render::semaphoreDestroy(context, semaphore_);
    released_ = true;
  }
}

VkSemaphore command_buffer_t::getSemaphore() 
{ 
  if(renderer_ && frameBuffer_ == renderer_->getBackBuffer() )
    return renderer_->getRenderCompleteSemaphore();

  return semaphore_; 
}

class render_task_t : public bkk::core::thread_pool_t::task_t
{
  public:
    
    render_task_t() {}
    void init(renderer_t* renderer, frame_buffer_handle_t framebuffer,
      VkCommandPool commandPool,
      actor_t* actors, uint32_t actorCount, const char* passName,
      VkSemaphore signalSemaphore, bool clear, const core::maths::vec4& clearColor, command_buffer_t* commandBuffer )
    {
      renderer_ = renderer;
      framebuffer_ = framebuffer;
      commandPool_ = commandPool;
      actors_ = actors;
      actorCount_ = actorCount;
      passName_ = passName;
      signalSemaphore_ = signalSemaphore;
      clear_ = clear;
      clearColor_ = clearColor;
      commandBuffer_ = commandBuffer;
    }

    void run()
    {
      commandBuffer_->init(renderer_, "render", signalSemaphore_, commandPool_);
      if (clear_)
      {
        commandBuffer_->clearRenderTargets(clearColor_);
      }

      commandBuffer_->render(actors_, actorCount_, passName_);
    }
  
  private:
    renderer_t* renderer_;
    actor_t* actors_;  
    uint32_t actorCount_;
    const char* passName_;
    frame_buffer_handle_t framebuffer_;
    VkCommandPool commandPool_;
    VkSemaphore signalSemaphore_;
    bool clear_;
    core::maths::vec4 clearColor_;
    
    command_buffer_t* commandBuffer_;
};


void bkk::framework::generateCommandBuffersParallel(renderer_t* renderer,
  frame_buffer_handle_t framebuffer,
  bool clear,
  core::maths::vec4 clearColor,
  const char* passName,
  actor_t* actors, uint32_t actorCount,
  VkSemaphore signalSemaphore,
  command_buffer_t** commandBuffers, uint32_t commandBufferCount)
{
  //1. Prepare pipelines (Can be done in parallel as well)
  framebuffer = (framebuffer != BKK_NULL_HANDLE) ? framebuffer : renderer->getBackBuffer();
  renderer->prepareShaders(passName, framebuffer);


  uint32_t actorsPerCommand = ( actorCount / commandBufferCount ) + 1;
  uint32_t currentActor = 0;

  std::vector<render_task_t> renderTask(commandBufferCount);
  
  *commandBuffers = new command_buffer_t[commandBufferCount];
  for (uint32_t i(0); i < commandBufferCount; ++i)
  {
    
    uint32_t count = (currentActor + actorsPerCommand < actorCount) ? actorsPerCommand :
                                                                      actorCount - currentActor;

    VkSemaphore signal = (i == commandBufferCount - 1) ? signalSemaphore : VK_NULL_HANDLE;

    renderTask[i].init(renderer, framebuffer,
      renderer->getCommandPool(i), actors, count, passName,
      signal, clear && i == 0, clearColor, *commandBuffers + i);

    actors += count;
    renderer->getThreadPool()->addTask(&renderTask[i]);
    currentActor += count;

  }

  renderer->getThreadPool()->waitForCompletion();

}