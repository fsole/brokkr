/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef RENDER_TARGET_H
#define RENDER_TARGET_H

#include <stdint.h>

#include "core/render.h"
#include "core/handle.h"

namespace bkk
{
  namespace framework
  {
    class renderer_t;
    typedef bkk::core::bkk_handle_t render_target_handle_t;

    class render_target_t
    {
    public:
      render_target_t();
      render_target_t(uint32_t width, uint32_t height, 
                      VkFormat format, 
                      bool depthBuffer,                      
                      renderer_t* renderer);

      void destroy(renderer_t* renderer);
            
      core::render::texture_t* getColorBuffer(){ return (core::render::texture_t*)&target_; }
      core::render::depth_stencil_buffer_t* getDepthStencilBuffer() { return &depthStencilBuffer_; }
      bool hasDepthBuffer() const { return hasDepthBuffer_; }

      uint32_t getWidth() const { return width_; }
      uint32_t getHeight() const { return height_; }
      VkFormat getFormat() const { return format_; }

    private:
      uint32_t width_;
      uint32_t height_;
      VkFormat format_;

      bool hasDepthBuffer_;

      core::render::texture_t target_;
      core::render::depth_stencil_buffer_t depthStencilBuffer_;      
    };

  }//framework
}//bkk

#endif