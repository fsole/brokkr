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

#include "application.h"
#include "maths.h"
#include "window.h"
#include "render.h"
#include "timer.h"
#include <string>
#include <sstream>
#include <iomanip>

using namespace bkk;
using namespace bkk::maths;


struct application_t::frame_counter_t
{
  void init(window::window_t* window, uint32_t displayInterval = 1000u)
  {
    window_ = window;
    windowTitle_ = window->title_;
    timePrev_ = timer::getCurrent();
    displayInterval_ = displayInterval;

    std::ostringstream titleWithFps;
    titleWithFps << windowTitle_ << "     0.0ms (0 fps)";
    window::setTitle(titleWithFps.str().c_str(), window_);
  }

  void endFrame()
  {
    timer::time_point_t currentTime = timer::getCurrent();
    float timeFrame = timer::getDifference(timePrev_, currentTime);
    timePrev_ = currentTime;
    timeAccum_ += timeFrame;
    if (timeAccum_ >= displayInterval_)
    {
      timeAccum_ = 0.0f;

      std::ostringstream titleWithFps;
      titleWithFps << windowTitle_ << "     " << std::setprecision(3) << timeFrame << "ms (" << (uint32_t)(1000 / timeFrame) << " fps)";
      window::setTitle(titleWithFps.str().c_str(), window_);
    }
  }

  window::window_t* window_;
  std::string windowTitle_;

  timer::time_point_t timePrev_;
  uint32_t displayInterval_;
  float timeAccum_ = 0.0f;
};

application_t::application_t(const char* title, u32 width, u32 height, u32 imageCount)
:timeDelta_(0),
 mouseCurrentPos_(0.0f,0.0f),
 mousePrevPos_(0.0f,0.0f),
 mouseButtonPressed_(false)
{
  window_ = new window::window_t();
  context_ = new render::context_t();

  window::create(title, width, height, window_);
  render::contextCreate(title, "", *window_, imageCount, context_);

  frameCounter_ = new frame_counter_t();
  frameCounter_->init(window_);
  timeDelta_ = 0.0f;
}

application_t::~application_t()
{
  render::contextDestroy(context_);
  window::destroy(window_);

  delete window_;
  delete context_;
  delete frameCounter_;
}

void application_t::loop()
{
  timer::time_point_t timePrev = bkk::timer::getCurrent();
  timer::time_point_t currentTime = timePrev;

  bool quit = false;
  while (!quit)
  {
    currentTime = bkk::timer::getCurrent();
    timeDelta_ = timer::getDifference(timePrev, currentTime);

    window::event_t* event = nullptr;
    while ((event = window::getNextEvent(window_)))
    {
      switch (event->type_)
      {
      case window::EVENT_QUIT:
      {
        quit = true;
        break;
      }
      case window::EVENT_RESIZE:
      {
        window::event_resize_t* resizeEvent = (window::event_resize_t*)event;
        render::swapchainResize(context_, resizeEvent->width_, resizeEvent->height_);
        onResize(resizeEvent->width_, resizeEvent->height_);
        break;
      }
      case window::EVENT_KEY:
      {
        window::event_key_t* keyEvent = (window::event_key_t*)event;
        onKeyEvent(keyEvent->keyCode_, keyEvent->pressed_);
        break;
      }
      case window::EVENT_MOUSE_BUTTON:
      {
        window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;

        mouseButtonPressed_ = buttonEvent->pressed_;
        vec2 prevPos = mouseCurrentPos_;
        mouseCurrentPos_ = vec2((float)buttonEvent->x_, (float)buttonEvent->y_);
        onMouseButton(buttonEvent->pressed_, mouseCurrentPos_, mousePrevPos_);
        mousePrevPos_ = prevPos;
        break;
      }
      case window::EVENT_MOUSE_MOVE:
      {
        window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
        vec2 prevPos = mouseCurrentPos_;
        mouseCurrentPos_ = vec2((float)moveEvent->x_, (float)moveEvent->y_);
        onMouseMove(mouseCurrentPos_, mouseCurrentPos_ - mousePrevPos_, mouseButtonPressed_);
        mousePrevPos_ = prevPos;
        break;
      }
      default:
        break;
      }
    }

    render();
    frameCounter_->endFrame();
    timePrev = currentTime;
  }

  render::contextFlush(*context_);
  onQuit();
}

render::context_t& application_t::getRenderContext() { return *context_; }
window::window_t& application_t::getWindow() { return *window_; }
f32 application_t::getTimeDelta() { return timeDelta_; }
maths::uvec2 application_t::getWindowSize() { return maths::uvec2(window_->width_, window_->height_); }


  