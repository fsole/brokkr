/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/application.h"
#include "framework/gui.h"

#include "core/maths.h"
#include "core/window.h"
#include "core/render.h"
#include "core/timer.h"


using namespace bkk;
using namespace bkk::framework;


class application_t::frame_counter_t
{
public:
  void init(core::window::window_t* window, uint32_t displayInterval = 1000u)
  {
    window_ = window;
    memcpy( windowTitle_, window->title, strlen(window->title) );
    timePrev_ = core::timer::getCurrent();
    displayInterval_ = displayInterval;

    char title[256];
    sprintf(title, "%s     0.0ms (0 fps)", windowTitle_);
    core::window::setTitle(title, window_);
  }

  void endFrame()
  {
    core::timer::time_point_t currentTime = core::timer::getCurrent();
    float timeFrame = core::timer::getDifference(timePrev_, currentTime);
    timePrev_ = currentTime;
    timeAccum_ += timeFrame;
    if (timeAccum_ >= displayInterval_)
    {
      timeAccum_ = 0.0f;

      uint32_t fps = (uint32_t)(1000 / timeFrame);

      char title[256];
      sprintf(title, "%s     %.2fms (%u fps)", windowTitle_, timeFrame, fps );
      core::window::setTitle(title, window_);
    }
  }
private:
  core::window::window_t* window_;
  char windowTitle_[128];

  core::timer::time_point_t timePrev_;
  uint32_t displayInterval_;
  float timeAccum_ = 0.0f;
};

application_t::application_t(const char* title, u32 width, u32 height, u32 imageCount)
:timeDelta_(0),
 mouseCurrentPos_(0.0f,0.0f),
 mousePrevPos_(0.0f,0.0f),
 mouseButtonPressed_(-1),
 running_(false)
{
  core::window::create(title, width, height, &window_);

  renderer_.initialize(title, imageCount, window_);

  frameCounter_ = new frame_counter_t();
  frameCounter_->init(&window_);
  timeDelta_ = 0.0f;

  framework::gui::init(renderer_.getContext());
}

application_t::~application_t()
{  
  core::window::destroy(&window_);
  delete frameCounter_;
}

void application_t::run()
{
  if (running_)
    return;

  running_ = true;
  core::timer::time_point_t timePrev = core::timer::getCurrent();
  core::timer::time_point_t currentTime = timePrev;

  bool quit = false;
  while (!quit)
  {
    currentTime = core::timer::getCurrent();
    timeDelta_ = core::timer::getDifference(timePrev, currentTime);

    core::window::event_t* event = nullptr;
    while ((event = core::window::getNextEvent(&window_)))
    {
      switch (event->type)
      {
      case core::window::EVENT_QUIT:
      {
        quit = true;
        break;
      }
      case core::window::EVENT_RESIZE:
      {
        core::window::event_resize_t* resizeEvent = (core::window::event_resize_t*)event;
        core::render::swapchainResize(&renderer_.getContext(), resizeEvent->width, resizeEvent->height);
        onResize(resizeEvent->width, resizeEvent->height);
        break;
      }
      case core::window::EVENT_KEY:
      {
        core::window::event_key_t* keyEvent = (core::window::event_key_t*)event;
        onKeyEvent(keyEvent->keyCode, keyEvent->pressed);
        break;
      }
      case core::window::EVENT_MOUSE_BUTTON:
      {
        core::window::event_mouse_button_t* buttonEvent = (core::window::event_mouse_button_t*)event;

        mouseButtonPressed_ = buttonEvent->pressed ? buttonEvent->button : -1;
        core::maths::vec2 prevPos = mouseCurrentPos_;
        mouseCurrentPos_ = core::maths::vec2((float)buttonEvent->x, (float)buttonEvent->y);
        onMouseButton(buttonEvent->button, buttonEvent->pressed, mouseCurrentPos_, mousePrevPos_);

        framework::gui::updateMouseButton(buttonEvent->button, buttonEvent->pressed);
        framework::gui::updateMousePosition(mouseCurrentPos_.x, mouseCurrentPos_.y);

        mousePrevPos_ = prevPos;
        break;
      }
      case core::window::EVENT_MOUSE_MOVE:
      {
        core::window::event_mouse_move_t* moveEvent = (core::window::event_mouse_move_t*)event;
        core::maths::vec2 prevPos = mouseCurrentPos_;
        mouseCurrentPos_ = core::maths::vec2((float)moveEvent->x, (float)moveEvent->y);
        onMouseMove(mouseCurrentPos_, mouseCurrentPos_ - mousePrevPos_ );

        framework::gui::updateMousePosition(mouseCurrentPos_.x, mouseCurrentPos_.y);

        mousePrevPos_ = prevPos;
        break;
      }
      default:
        break;
      }
    }

    framework::gui::beginFrame(renderer_.getContext());
    buildGuiFrame();
    framework::gui::endFrame();

    render();

    frameCounter_->endFrame();
    timePrev = currentTime;
  }

  core::render::contextFlush(renderer_.getContext());
  framework::gui::destroy(renderer_.getContext());
  onQuit();
}

void application_t::beginFrame()
{
  renderer_.update();
}

void application_t::presentFrame()
{
  renderer_.presentFrame();
}


  