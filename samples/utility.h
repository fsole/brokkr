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

#ifndef UTILITY_H
#define UTILITY_H

#include "mesh.h"
#include "render.h"
#include "maths.h"
#include "timer.h"
#include "window.h"
#include <string>
#include <sstream>
#include <iomanip>

using namespace bkk;
using namespace maths;

namespace sample_utils
{

struct orbiting_camera_t
{
  orbiting_camera_t()
  :offset_(0.0f),
   angle_(vec2(0.0f,0.0f)),
   rotationSensitivity_(0.01f)
  {
    Update();
  }

  orbiting_camera_t( const f32 offset, const vec2& angle, f32 rotationSensitivity )
  :offset_(offset),
   angle_(angle),
   rotationSensitivity_(rotationSensitivity)
  {
    Update();
  }

  void Move( f32 amount )
  {
    offset_ += amount;
    if( offset_ < 0.0f )
    {
      offset_ = 0.0f;
    }

    Update();
  }

  void Rotate( f32 angleY, f32 angleZ )
  {
    angle_.x = angle_.x + angleY * rotationSensitivity_;
    angle_.y = angle_.y + angleZ * rotationSensitivity_;
    Update();
  }

  void Update()
  {
    quat orientation =  quaternionFromAxisAngle( vec3(1.0f,0.0f,0.0f), angle_.y ) *
                        quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), angle_.x);
          
    invertMatrix( createTransform(vec3(0.0f,0.0f,offset_), VEC3_ONE, QUAT_UNIT) * createTransform( VEC3_ZERO, VEC3_ONE, orientation ), view_ );
  }

  mat4 view_;
  f32 offset_;
  vec2 angle_;
  f32 rotationSensitivity_;
};


struct free_camera_t
{
  free_camera_t()
  :position_(0.0f,0.0f,0.0f),
   angle_(0.0f,0.0f),
   velocity_(1.0f),
   rotationSensitivity_(0.01f)
  {
    Update();
  }

  free_camera_t(const vec3& position, const vec2& angle, f32 velocity, f32 rotationSensitivity)
    :position_(position),
    angle_(angle),
    velocity_(velocity),
    rotationSensitivity_(rotationSensitivity)
  {
    Update();
  }

  void Move( f32 xAmount, f32 zAmount )
  {
    position_ = position_ + (zAmount * velocity_ * tx_.row(2).xyz()) +(xAmount * velocity_ * tx_.row(0).xyz() );
    Update();
  }

  void Rotate( f32 angleY, f32 angleX )
  {    
    angle_.y = angle_.y + angleY * rotationSensitivity_;
    angleX = angle_.x + angleX * rotationSensitivity_;
    if(angleX < M_PI_2 && angleX > -M_PI_2 )
    {
      angle_.x = angleX;
    }

    Update();
  }

  void Update()
  {
    quat orientation = quaternionFromAxisAngle(vec3(1.0f, 0.0f, 0.0f), angle_.x) * quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), angle_.y);
    tx_ = createTransform( position_, VEC3_ONE, orientation );
    invertMatrix( tx_, view_ );
  }

  void LookAt(const vec3& eye, const vec3& center, const vec3& up)
  {
    view_ = maths::lookAtMatrix(eye, center, up );
    tx_ = maths::invertTransform(view_);
  }

  mat4 tx_;
  mat4 view_;
  vec3 position_;  
  vec2 angle_;
  f32  velocity_; //Units per second
  f32 rotationSensitivity_;
};

bkk::mesh::mesh_t fullScreenQuad(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: IN Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { { { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f } },
                                     { {  1.0f,  1.0f, 0.0f },{ 1.0f, 1.0f } },
                                     { {  1.0f, -1.0f, 0.0f },{ 1.0f, 0.0f } },
                                     { { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f } }
  };

  static const uint32_t indices[] = { 0,1,2,0,2,3 };
  


  static bkk::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = bkk::render::vertex_attribute_t::format::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr,  &mesh);
  return mesh;
}


struct frame_counter_t
{
  typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point_t;

  void init( window::window_t* window, uint32_t displayInterval = 1000u )
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
    time_point_t t = timer::getCurrent();
    float timeFrame = timer::getDifference(timePrev_, t);
    timePrev_ = t;
    timeAccum_ += timeFrame;
    if (timeAccum_ >= displayInterval_ )
    {
      timeAccum_ = 0.0f;

      std::ostringstream titleWithFps;
      titleWithFps << windowTitle_ << "     " << std::setprecision(3)  << timeFrame << "ms (" << (uint32_t)(1000 / timeFrame ) << " fps)";
      window::setTitle(titleWithFps.str().c_str(), window_ );
    }
  };


  window::window_t* window_;
  std::string windowTitle_;

  time_point_t timePrev_;
  uint32_t displayInterval_;
  float timeAccum_ = 0.0f;
  
};


class application_t
{
public:
  application_t( const char* title, u32 width, u32 height, u32 imageCount )
  {
    window::create(title, width, height, &window_);
    render::contextCreate(title, "", window_, imageCount, &context_);
    frameCounter_.init(&window_);
    timeDelta_ = 0.0f;
  }

  ~application_t()
  {
    render::contextDestroy(&context_);
    window::destroy(&window_);
  }

  void loop()
  {
    auto timePrev = bkk::timer::getCurrent();
    auto currentTime = timePrev;

    bool quit = false;
    while (!quit)
    { 
      currentTime = bkk::timer::getCurrent();
      timeDelta_ = timer::getDifference(timePrev, currentTime);
      
      window::event_t* event = nullptr;
      while ((event = window::getNextEvent(&window_)))
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
          render::swapchainResize(&context_, resizeEvent->width_, resizeEvent->height_);
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
          onMouseMove( mouseCurrentPos_, mouseCurrentPos_ - mousePrevPos_, mouseButtonPressed_ );
          mousePrevPos_ = prevPos;
          break;
        }
        default:
          break;
        }
      }

      render();      
      frameCounter_.endFrame();
      timePrev = currentTime;
    }

    render::contextFlush(context_);
    onQuit();
  }
  
  //Callbacks
  virtual void onQuit() {}
  virtual void onResize(u32 width, u32 height) {}
  virtual void onKeyEvent(window::key_e key, bool pressed) {}
  virtual void onMouseButton(bool pressed, const vec2& mousePos, const vec2& mousePrevPos) {}
  virtual void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos, bool buttonPressed) {}
  virtual void render() {}

 
  render::context_t& getRenderContext() { return context_;  }
  window::window_t& getWindow() { return window_;  }
  f32 getTimeDelta() { return timeDelta_; }  
  uvec2 getWindowSize() { return uvec2(window_.width_, window_.height_); }

private:
  window::window_t window_;
  render::context_t context_;

  float timeDelta_;
  frame_counter_t frameCounter_;
  maths::vec2 mouseCurrentPos_ = vec2(0.0f, 0.0f);
  maths::vec2 mousePrevPos_ = vec2(0.0f, 0.0f);
  bool mouseButtonPressed_ = false;

};

}

#endif  /*UTILITY_H*/