/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/maths.h"
#include "core/window.h"

#include "framework/gui.h"
#include "framework/renderer.h"

namespace bkk
{
  namespace framework
  {
    class application_t
    {
      public:
        application_t(const char* title, u32 width, u32 height, u32 imageCount);
        ~application_t();

        void run();        

      protected:
        void loop();
        renderer_t& getRenderer() { return renderer_; }
        core::render::context_t& getRenderContext() { return renderer_.getContext(); }
        core::window::window_t& getWindow() { return window_; }
        f32 getTimeDelta() { return timeDelta_; }
        core::maths::uvec2 getWindowSize() { return core::maths::uvec2(window_.width, window_.height); }
        f32 getAspectRatio() { return (window_.width / (float)window_.height); }


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

        void beginFrame();
        void presentFrame();
        
      private:
        framework::renderer_t renderer_;
        core::window::window_t window_;
        float timeDelta_;
        core::maths::vec2 mouseCurrentPos_;
        core::maths::vec2 mousePrevPos_;
        s32 mouseButtonPressed_;

        class frame_counter_t;
        frame_counter_t* frameCounter_;

        application_t();
    };

  }//framework
}//bkk

#endif