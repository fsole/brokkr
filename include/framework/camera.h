/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef CAMERA_H
#define CAMERA_H

#include "core/maths.h"
#include "core/render.h"
#include "core/packed-freelist.h"

namespace bkk
{
  namespace framework
  {
    class actor_t;
    class renderer_t;
    typedef core::handle_t camera_handle_t;

    class camera_t
    {
    public:
      enum projection_mode_e
      {
        PERSPECTIVE_PROJECTION = 0,
        ORTHOGRAPHIC_PROJECTION = 1
      };

      camera_t();
      camera_t(projection_mode_e projectionMode, float fov, float aspect, float nearPlane, float farPlane);

      void update(renderer_t* renderer);
      void cull(actor_t* actors, uint32_t actorCount);
      void destroy(renderer_t* renderer);

      uint32_t getVisibleActors(actor_t** actors);
      core::render::gpu_buffer_t getUniformBuffer() { return uniformBuffer_; }
      core::render::descriptor_set_t getDescriptorSet() { return descriptorSet_; }
      void setWorldToViewMatrix(core::maths::mat4& m);
      void setViewToWorldMatrix(core::maths::mat4& m);

    private:
      struct uniforms_t
      {
        core::maths::mat4 worldToView;
        core::maths::mat4 viewToWorld;
        core::maths::mat4 projection;
        core::maths::mat4 projectionInverse;
      }uniforms_;

      core::render::gpu_buffer_t uniformBuffer_ = {};
      core::render::descriptor_set_t descriptorSet_ = {};

      projection_mode_e projection_;
      float fov_;
      float aspect_;
      float nearPlane_;
      float farPlane_;
      
      uint32_t visibleActorsCount_ = 0u;
      actor_t* visibleActors_ = nullptr;
    };

    class orbiting_camera_controller_t
    {
    public:
      orbiting_camera_controller_t();
      orbiting_camera_controller_t(const core::maths::vec3& target, const f32 offset, const core::maths::vec2& angle, f32 rotationSensitivity);
      
      void setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer);

      void Move(f32 amount);
      void Rotate(f32 angleY, f32 angleZ);
      void Update();

      const core::maths::mat4& getViewMatrix() const { return view_; }

    private:
      core::maths::mat4 view_;
      core::maths::vec3 target_;
      f32 offset_;
      core::maths::vec2 angle_;
      f32 rotationSensitivity_;

      camera_handle_t cameraHandle_;
      renderer_t* renderer_;
    };

    class free_camera_controller_t
    {
    public:
      free_camera_controller_t();
      free_camera_controller_t(const core::maths::vec3& position, const core::maths::vec2& angle, f32 velocity, f32 rotationSensitivity);

      void setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer);

      void Move(f32 xAmount, f32 zAmount);
      void Rotate(f32 angleY, f32 angleX);
      void Update();

      const core::maths::mat4& getViewMatrix() const{ return view_; }
      const core::maths::mat4& getWorldMatrix() const{ return tx_; }
      
      void setPosition(const core::maths::vec3& position) { position_ = position; }
      const core::maths::vec3& getPosition() { return position_; }
      
      void setRotation(const core::maths::vec2& angle) { angle_ = angle; }
      const core::maths::vec2& getRotation() { return angle_; }

    private:
      core::maths::mat4 tx_;
      core::maths::mat4 view_;
      core::maths::vec3 position_;
      core::maths::vec2 angle_;
      f32  velocity_; //Units per second
      f32 rotationSensitivity_;

      camera_handle_t cameraHandle_;
      renderer_t* renderer_;
    };

  }//framework
}//bkk


#endif