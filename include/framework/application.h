
/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/maths.h"

namespace bkk
{
  //Forward declarations
  namespace core
  {
    namespace render { struct context_t; }
    namespace window { struct window_t; }
  }

  namespace framework
  {
    class application_t
    {
    public:
      application_t(const char* title, u32 width, u32 height, u32 imageCount);
      ~application_t();

      void loop();

      core::render::context_t& getRenderContext();

      core::window::window_t& getWindow();
      core::maths::uvec2 getWindowSize();
      f32 getAspectRatio();

      f32 getTimeDelta();

      core::maths::vec2 getMousePosition() { return mouseCurrentPos_; }
      s32 getMousePressedButton() { return mouseButtonPressed_; }

      //Callbacks
      virtual void onQuit() {}
      virtual void onResize(u32 width, u32 height) {}
      virtual void onKeyEvent(u32 key, bool pressed) {}
      virtual void onMouseButton(u32 button, bool pressed, const core::maths::vec2& mousePos, const core::maths::vec2& mousePrevPos) {}
      virtual void onMouseMove(const core::maths::vec2& mousePos, const core::maths::vec2& mouseDeltaPos) {}
      virtual void buildGuiFrame() {}
      virtual void render() {}

    private:
      core::window::window_t* window_;
      core::render::context_t* context_;

      float timeDelta_;
      core::maths::vec2 mouseCurrentPos_;
      core::maths::vec2 mousePrevPos_;
      s32 mouseButtonPressed_;

      struct frame_counter_t;
      frame_counter_t* frameCounter_;

      application_t();
    };
  }
}

#endif