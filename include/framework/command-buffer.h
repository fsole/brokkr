#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H


#include "core/maths.h"
#include "core/render.h"
#include "framework/frame-buffer.h"
#include "framework/material.h"

namespace bkk
{
  namespace framework
  {
    class renderer_t;
    struct actor_t;

    class command_buffer_t
    {
      public:
        
        command_buffer_t(renderer_t* renderer,                          
                         frame_buffer_handle_t frameBuffer = core::NULL_HANDLE,
                         command_buffer_t* prevCommandBuffer = nullptr);

        command_buffer_t(const command_buffer_t& cmdBuffer );

        ~command_buffer_t();

        void clearRenderTargets(core::maths::vec4 color);
        
        void render(actor_t* actors, uint32_t actorCount, const char* passName );
        void blit(render_target_handle_t renderTarget, material_handle_t materialHandle = core::NULL_HANDLE, const char* pass = nullptr);
        
        void submit();
        void release();
        
        void cleanup();
        VkSemaphore* getSemaphore();

      private:
        void beginCommandBuffer();
        command_buffer_t();

        renderer_t* renderer_;

        frame_buffer_handle_t frameBuffer_;

        core::render::command_buffer_t commandBuffer_;
        VkSemaphore semaphore_;

        core::maths::vec4 clearColor_;
        bool clear_;
        bool released_;
    };
  }
}

#endif
