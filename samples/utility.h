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

#include "maths.h"

using namespace bkk;
using namespace maths;

namespace sample_utils
{

struct orbiting_camera_t
{
  orbiting_camera_t()
  :offset_(0.0f),
   angle_(vec2(0.0f,0.0f))
  {
    Update();
  }

  orbiting_camera_t( const f32 offset, const vec2& angle )
  :offset_(offset),
   angle_(angle)
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
    angle_.x =  angle_.x + angleY;
    angleZ = angle_.y + angleZ;
    if( angleZ < M_PI_2 && angleZ > -M_PI_2 )
    {
      angle_.y = angleZ;
    }

    Update();
  }

  void Update()
  {
    quat orientation =  quaternionFromAxisAngle( vec3(1.0f,0.0f,0.0f), angle_.y ) *
                        quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), angle_.x);

    forward_ = maths::rotate( vec3(0.0f,0.0f,1.0f), orientation );
    right_ = cross( forward_, vec3(0.0f,1.0f,0.0f) );
    computeInverse( computeTransform(vec3(0.0f,0.0f,offset_), VEC3_ONE, QUAT_UNIT) * computeTransform( VEC3_ZERO, VEC3_ONE, orientation ), view_ );
  }

  mat4 view_;
  f32 offset_;
  vec2 angle_;
  vec3 forward_;
  vec3 right_;
};


struct free_camera_t
{
  free_camera_t()
  :position_(0.0f,0.0f,0.0f),
   angle_(0.0f,0.0f),
   velocity_(1.0f)
  {
    Update();
  }

  free_camera_t( const vec3& position, const vec2& angle, f32 velocity )
  :position_(position),
   angle_(angle),
   velocity_(velocity)
  {
    Update();
  }

  void Move( f32 xAmount, f32 zAmount )
  {
    position_ = position_ + (zAmount * velocity_ * tx_.row(2).xyz()) +(xAmount * velocity_ * tx_.row(0).xyz() );
    Update();
  }

  void Rotate( f32 angleX, f32 angleY )
  {    
    angle_.y = angle_.y + angleY;
    angleX = angle_.x + angleX;
    if(angleX < M_PI_2 && angleX > -M_PI_2 )
    {
      angle_.x = angleX;
    }

    Update();
  }

  void Update()
  {
    quat orientation = quaternionFromAxisAngle(vec3(1.0f, 0.0f, 0.0f), angle_.x) * quaternionFromAxisAngle(vec3(0.0f, 1.0f, 0.0f), angle_.y);
    tx_ = computeTransform( position_, VEC3_ONE, orientation );
    computeInverse( tx_, view_ );    
  }

  mat4 tx_;
  mat4 view_;
  vec3 position_;  
  vec2 angle_;
  f32  velocity_; //Units per second
};

bkk::mesh::mesh_t FullScreenQuad(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: IN Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { { { -1.0f, 1.0f, 0.0f },{ 0.0f, 1.0f } },
  { { 1.0f,  1.0f, 0.0f },{ 1.0f, 1.0f } },
  { { 1.0f, -1.0f, 0.0f },{ 1.0f, 0.0f } },
  { { -1.0f,-1.0f, 1.0f },{ 0.0f, 0.0f } }
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
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh);
  return mesh;
}

}

#endif  /*UTILITY_H*/