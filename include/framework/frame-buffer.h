/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "framework/render-target.h"
#include <vector>

namespace bkk
{
  namespace framework
  {
    class renderer;

    typedef bkk::core::bkk_handle_t frame_buffer_bkk_handle_t;

    class frame_buffer_t
    {
      public:
        frame_buffer_t();
        frame_buffer_t(render_target_bkk_handle_t* renderTargets, uint32_t targetCount,
          VkImageLayout* initialLayouts, VkImageLayout* finalLayouts, renderer_t* renderer );

        void destroy(renderer_t* renderer);

        uint32_t getWidth() const { return width_; }
        uint32_t getHeight() const { return height_; }
        uint32_t getTargetCount() const { return targetCount_; }
        core::render::render_pass_t getRenderPass() { return renderPass_; }
        core::render::render_pass_t getRenderPassNoClear() { return renderPassNoClear_; }
        core::render::frame_buffer_t getFrameBuffer() { return frameBuffer_; }

      private:
        core::render::render_pass_t renderPass_;
        core::render::render_pass_t renderPassNoClear_;
        core::render::frame_buffer_t frameBuffer_;

        uint32_t width_;
        uint32_t height_;
        uint32_t targetCount_;

        render_target_bkk_handle_t* renderTargets_;
      };

  }//framework
}//bkk


#endif