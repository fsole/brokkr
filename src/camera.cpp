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

#include "camera.h"

using namespace bkk;
using namespace bkk::camera;

orbiting_camera_t::orbiting_camera_t()
:offset_(0.0f),
 angle_(maths::vec2(0.0f, 0.0f)),
 rotationSensitivity_(0.01f)
{
  Update();
}

orbiting_camera_t::orbiting_camera_t(const f32 offset, const maths::vec2& angle, f32 rotationSensitivity)
:offset_(offset),
 angle_(angle),
 rotationSensitivity_(rotationSensitivity)
{
  Update();
}

void orbiting_camera_t::Move(f32 amount)
{
  offset_ += amount;
  if (offset_ < 0.0f)
  {
    offset_ = 0.0f;
  }

  Update();
}

void orbiting_camera_t::Rotate(f32 angleY, f32 angleZ)
{
  angle_.x = angle_.x + angleY * rotationSensitivity_;
  angle_.y = angle_.y + angleZ * rotationSensitivity_;
  Update();
}

void orbiting_camera_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.y) *
    maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.x);

  maths::invertMatrix(maths::createTransform(maths::vec3(0.0f, 0.0f, offset_), maths::VEC3_ONE, maths::QUAT_UNIT) * maths::createTransform(maths::VEC3_ZERO, maths::VEC3_ONE, orientation), view_);
}


free_camera_t::free_camera_t()
:position_(0.0f, 0.0f, 0.0f),
 angle_(0.0f, 0.0f),
 velocity_(1.0f),
 rotationSensitivity_(0.01f)
{
  Update();
}

free_camera_t::free_camera_t(const maths::vec3& position, const maths::vec2& angle, f32 velocity, f32 rotationSensitivity)
  :position_(position),
  angle_(angle),
  velocity_(velocity),
  rotationSensitivity_(rotationSensitivity)
{
  Update();
}

void free_camera_t::Move(f32 xAmount, f32 zAmount)
{
  position_ = position_ + (zAmount * velocity_ * tx_.row(2).xyz()) + (xAmount * velocity_ * tx_.row(0).xyz());
  Update();
}

void free_camera_t::Rotate(f32 angleY, f32 angleX)
{
  angle_.y = angle_.y + angleY * rotationSensitivity_;
  angleX = angle_.x + angleX * rotationSensitivity_;
  if (angleX < PI_2 && angleX > -PI_2)
  {
    angle_.x = angleX;
  }

  Update();
}

void free_camera_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.x) * maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.y);
  tx_ = maths::createTransform(position_, maths::VEC3_ONE, orientation);
  maths::invertMatrix(tx_, view_);
}