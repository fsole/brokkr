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

    class command_buffer_t
    {
      public:
        command_buffer_t();
        command_buffer_t(renderer_t* renderer );
        ~command_buffer_t();
        
        void setDependencies(command_buffer_t* prevCommandBuffers, uint32_t count);
        void setFrameBuffer(frame_buffer_bkk_handle_t frameBuffer);

        void clearRenderTargets(core::maths::vec4 color);
        
        void render(actor_t* actors, uint32_t actorCount, const char* passName );
        void blit(render_target_bkk_handle_t renderTarget, material_bkk_handle_t materialHandle = core::BKK_NULL_HANDLE, const char* pass = nullptr);
        void blit(const bkk::core::render::texture_t& texture, material_bkk_handle_t materialHandle = core::BKK_NULL_HANDLE, const char* pass = nullptr);
        
        void dispatchCompute(compute_material_bkk_handle_t computeMaterial, uint32_t pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);
        void dispatchCompute(compute_material_bkk_handle_t computeMaterial, const char* pass, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

        void submit();
        void release();
        
        void cleanup();
        VkSemaphore getSemaphore();

      private:
        enum type_e {
          GRAPHICS,
          COMPUTE
        };

        void beginCommandBuffer();
        void createCommandBuffer(type_e type);

        renderer_t* renderer_;
        std::vector<command_buffer_t> dependencies_;
        core::render::command_buffer_t commandBuffer_;
        VkSemaphore semaphore_;
        frame_buffer_bkk_handle_t frameBuffer_;
        core::maths::vec4 clearColor_;
        bool clear_;
        bool released_;

    };

  }//framework
}//bkk

#endif
