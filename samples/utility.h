
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
    angle_.x =  angle_.x - angleY;
    angleZ = angle_.y - angleZ;
    if( angleZ < M_PI_2 && angleZ > -M_PI_2 )
    {
      angle_.y = angleZ;
    }

    Update();
  }

  void Update()
  {
    quat orientation =  quaternionFromAxisAngle( vec3(0.0f,1.0f,0.0f), angle_.x ) *
                        quaternionFromAxisAngle( vec3(1.0f,0.0f,0.0f), angle_.y );

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
    position_ = position_ + ( zAmount * velocity_ * forward_ ) + ( xAmount * velocity_ * right_ );
    Update();
  }

  void Rotate( f32 angleX, f32 angleY )
  {
    angle_.x =  angle_.x - angleY;
    angleY = angle_.y - angleX;
    if( angleY < M_PI_2 && angleY > -M_PI_2 )
    {
      angle_.y = angleY;
    }

    Update();
  }

  void Update()
  {
    quat orientation =  quaternionFromAxisAngle( vec3(0.0f,1.0f,0.0f), angle_.x ) *
                        quaternionFromAxisAngle( vec3(1.0f,0.0f,0.0f), angle_.y );

    forward_ = normalize( maths::rotate( vec3(0.0f,0.0f,1.0f), orientation ) );
    right_ = normalize( cross( forward_, vec3(0.0f,1.0f,0.0f) ) );

    tx_ = computeTransform( position_, VEC3_ONE, orientation );
    computeInverse( tx_, view_ );
  }

  mat4 tx_;
  mat4 view_;

  vec3 position_;
  vec3 forward_;
  vec3 right_;
  vec2 angle_;
  f32  velocity_; //Units per second
};

}

#endif  /*UTILITY_H*/