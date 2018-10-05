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

#ifndef CAMERA_H
#define CAMERA_H

#include "core/maths.h"

namespace bkk
{
  namespace framework
  {
    struct orbiting_camera_t
    {
      orbiting_camera_t();
      orbiting_camera_t(const core::maths::vec3& target, const f32 offset, const core::maths::vec2& angle, f32 rotationSensitivity);

      void Move(f32 amount);
      void Rotate(f32 angleY, f32 angleZ);
      void Update();

      core::maths::mat4 view_;
      core::maths::vec3 target_;
      f32 offset_;
      core::maths::vec2 angle_;
      f32 rotationSensitivity_;
    };


    struct free_camera_t
    {
      free_camera_t();
      free_camera_t(const core::maths::vec3& position, const core::maths::vec2& angle, f32 velocity, f32 rotationSensitivity);

      void Move(f32 xAmount, f32 zAmount);
      void Rotate(f32 angleY, f32 angleX);
      void Update();

      core::maths::mat4 tx_;
      core::maths::mat4 view_;
      core::maths::vec3 position_;
      core::maths::vec2 angle_;
      f32  velocity_; //Units per second
      f32 rotationSensitivity_;
    };
  }

}


#endif