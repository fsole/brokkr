/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef GUI_H
#define GUI_H

#include "core/render.h"
#include "../external/imgui/imgui.h"

namespace bkk
{
  namespace framework
  {
    namespace gui
    {
      void init(const core::render::context_t& context);
      void destroy(const core::render::context_t& context);

      void beginFrame(const core::render::context_t& context);
      void endFrame();
      void draw(const core::render::context_t& context, core::render::command_buffer_t commandBuffer);

      void updateMousePosition(float x, float y);
      void updateMouseButton(uint32_t button, bool pressed);
    }

  }//framework
}//bkk

#endif

