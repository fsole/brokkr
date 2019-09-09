/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H


#include "core/maths.h"
#include "core/render.h"
#include "framework/frame-buffer.h"
#include "framework/material.h"
#include "framework/compute-material.h"

namespace bkk
{
  namespace framework
  {
    class renderer_t;
    class actor_t;

    struct layout_transition_t
    {
      layout_transition_t(core::render::texture_t* texture, VkImageLayout layout,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
        :texture(texture), renderTarget( core::BKK_NULL_HANDLE), layout(layout), srcStageMask(srcStageMask), dstStageMask(dstStageMask)
      {
      }

      layout_transition_t(render_target_handle_t renderTarget, VkImageLayout layout,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
        :texture(nullptr), renderTarget(renderTarget), layout(layout), srcStageMask(srcStageMask), dstStageMask(dstStageMask)
      {
      }

      core::render::texture_t* texture;
      render_target_handle_t renderTarget;
      VkImageLayout layout;
      VkPipelineStageFlags srcStageMask;
      VkPipelineStageFlags dstStageMask;
    };

    class command_buffer_t
    {
      public:
        command_buffer_t();

        command_buffer_t(renderer_t* renderer, const char* name = nullptr, VkSemaphore signalSemaphore = VK_NULL_HANDLE, VkCommandPool pool = VK_NULL_HANDLE);
        ~command_buffer_t();
        
        void init(renderer_t* renderer, const char* name = nullptr, VkSemaphore signalSemaphore = VK_NULL_HANDLE, VkCommandPool pool = VK_NULL_HANDLE);
        void setDependencies(command_buffer_t* prevCommandBuffers, uint32_t count);
        void setFrameBuffer(frame_buffer_handle_t frameBuffer);

        void clearRenderTargets(const core::maths::vec4& color);
        
        void render(actor_t* actors, uint32_t actorCount, const char* passName );
        void blit(render_target_handle_t renderTarget, material_handle_t materialHandle = core::BKK_NULL_HANDLE, const char* pass = nullptr);
        void blit(const bkk::core::render::texture_t& texture, material_handle_t materialHandle = core::BKK_NULL_HANDLE, const char* pass = nullptr);

        void changeLayout(render_target_handle_t renderTarget, VkImageLayout layout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void changeLayout(core::render::texture_t* texture, VkImageLayout layout, VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        void changeLayout(const layout_transition_t* transitions, uint32_t count);

        void dispatchCompute(compute_material_handle_t computeMaterial, uint32_t pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
        void dispatchCompute(compute_material_handle_t computeMaterial, const char* pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

        void submit();
        void release();

        void submitAndRelease();
        
        void cleanup();
        VkSemaphore getSemaphore();

      private:
        enum type_e {
          GRAPHICS,
          COMPUTE
        };

        void beginCommandBuffer();
        void endCommandBuffer();
        void createCommandBuffer(type_e type);

        renderer_t* renderer_;
        std::string name_;

        std::vector<command_buffer_t> dependencies_;
        core::render::command_buffer_t commandBuffer_;
        VkSemaphore semaphore_;
        VkCommandPool commandPool_;

        frame_buffer_handle_t frameBuffer_;
        core::maths::vec4 clearColor_;
        bool clear_;
        bool released_;
        VkSemaphore signalSemaphore_;
    };

    //Generates a number of "render command-buffers" in parallel
    void generateCommandBuffersParallel(renderer_t* renderer,
      const char* name,
      frame_buffer_handle_t framebuffer,
      const core::maths::vec4* clearColor,
      actor_t* actors, uint32_t actorCount,
      const char* passName,
      VkSemaphore signalSemaphore,
      command_buffer_t* prevCommandBuffers, uint32_t count,
      layout_transition_t* layoutTransitions, uint32_t layoutTransitionsCount,
      command_buffer_t* commandBuffers, uint32_t commandBufferCount);


  }//framework
}//bkk

#endif
