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
        core::maths::mat4 worldToView_;
        core::maths::mat4 viewToWorld_;
        core::maths::mat4 projection_;
        core::maths::mat4 projectionInverse_;
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
      void setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer);

      void Move(f32 xAmount, f32 zAmount);
      void Rotate(f32 angleY, f32 angleX);
      void Update();

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