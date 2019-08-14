/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/maths.h"
#include "core/handle.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/thread-pool.h"
#include "core/timer.h"

#include "framework/camera.h"
#include "framework/actor.h"
#include "framework/renderer.h"

#define MIN_ACTOR_COUNT_FOR_PARALLEL_CULLING 10
#define CULL_TASK_COUNT 4

using namespace bkk::core;
using namespace bkk::framework;

camera_t::camera_t()
{}

camera_t::camera_t(projection_mode_e projectionMode, float fov, float aspect, float nearPlane, float farPlane)
:projection_(projectionMode),
fov_(fov),
aspect_(aspect),
nearPlane_(nearPlane),
farPlane_(farPlane)
{
  if (projection_ == camera_t::PERSPECTIVE_PROJECTION)
  {
    uniforms_.projection = maths::perspectiveProjectionMatrix(fov_, aspect_, nearPlane_, farPlane_);
  }
  else
  {
    uniforms_.projection = maths::orthographicProjectionMatrix(-fov_, fov_, fov_, -fov_, nearPlane_, farPlane_);
  }

  maths::invertMatrix(uniforms_.projection, &uniforms_.projectionInverse);
}

void camera_t::update(renderer_t* renderer)
{ 
  render::context_t& context = renderer->getContext();
  if (uniformBuffer_.handle == VK_NULL_HANDLE)
  {
    //Create buffer
    render::gpuBufferCreate(context, render::gpu_buffer_t::UNIFORM_BUFFER,
      (void*)&uniforms_, sizeof(uniforms_),
      nullptr, &uniformBuffer_);

    render::descriptor_t descriptor = render::getDescriptor(uniformBuffer_);
    render::descriptorSetCreate(context, renderer->getDescriptorPool(), renderer->getGlobalsDescriptorSetLayout(), &descriptor, &descriptorSet_);
  }
  else
  {
    render::gpuBufferUpdate(context, (void*)&uniforms_,
      0u, sizeof(uniforms_),
      &uniformBuffer_);
  }
}

class cullTask : public thread_pool_t::task_t
{
  public:
  void init(renderer_t* renderer, actor_t* actors, uint32_t actorCount, maths::vec4* frustum, bool* results)
  {
    renderer_ = renderer;
    actors_ = actors;
    actorCount_ = actorCount;
    frustum_ = frustum;
    results_ = results;
  }

  void run()
  {
    for (uint32_t i = 0; i < actorCount_; ++i)
    {
      mesh::mesh_t* mesh = renderer_->getMesh(actors_[i].getMeshHandle());
      if (mesh)
      {
        //Transform aabb to world space
        maths::aabb_t aabbWS = maths::aabbTransform(mesh->aabb, *renderer_->getTransform(actors_[i].getTransformHandle()));
        results_[i] = aabbInFrustum(aabbWS, frustum_);
      }
    }
  }

  renderer_t* renderer_;
  actor_t* actors_;
  uint32_t actorCount_;
  maths::vec4* frustum_;

  bool* results_;
};

void camera_t::cull(renderer_t* renderer, actor_t* actors, uint32_t actorCount)
{ 
  timer::scoped_timer_t t("CUlling");
  //Extract frustum planes in world space
  maths::vec4 frustumWS[6];
  maths::frustumPlanesFromMatrix(uniforms_.worldToView * uniforms_.projection, &frustumWS[0]);

  visibleActorsCount_ = 0u;
  visibleActors_.resize(actorCount);
  
  if (actorCount > MIN_ACTOR_COUNT_FOR_PARALLEL_CULLING)
  {    
    std::vector<cullTask> cullTasks(CULL_TASK_COUNT);
    uint32_t actorsPerCullTask = actorCount / CULL_TASK_COUNT;
    uint32_t currentActor = 0;
    bool* cullingResults = new bool[actorCount];
    for (uint32_t i(0); i<CULL_TASK_COUNT; ++i)
    {
      uint32_t count = (i < CULL_TASK_COUNT-1) ? actorsPerCullTask :
                                                 actorCount - currentActor;

      cullTasks[i].init(renderer, actors + currentActor, count, frustumWS, cullingResults + currentActor );
      currentActor += count;

      renderer->getThreadPool()->addTask(&cullTasks[i]);
    }

    renderer->getThreadPool()->waitForCompletion();

    for (uint32_t i(0); i < actorCount; ++i)
    {
      if (cullingResults[i])
        visibleActors_[visibleActorsCount_++] = actors[i];
    }

    delete[] cullingResults;
  }
  else
  {
    for (uint32_t i = 0; i < actorCount; ++i)
    {
      mesh::mesh_t* mesh = renderer->getMesh(actors[i].getMeshHandle());
      if (mesh)
      {
        //Transform aabb to world space
        maths::aabb_t aabbWS = maths::aabbTransform(mesh->aabb, *renderer->getTransform(actors[i].getTransformHandle()));
        if (aabbInFrustum(aabbWS, frustumWS))
          visibleActors_[visibleActorsCount_++] = actors[i];
      }
    }

    visibleActors_.resize(visibleActorsCount_);
  }
}

void camera_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();
  if (uniformBuffer_.handle != VK_NULL_HANDLE)
  {
    render::gpuBufferDestroy(context, nullptr, &uniformBuffer_);
    render::descriptorSetDestroy(context, &descriptorSet_);
  }
}

uint32_t camera_t::getVisibleActors(actor_t** actors)
{
  *actors = visibleActors_.data();
  return visibleActorsCount_;
}

void camera_t::setViewToWorldMatrix(const maths::mat4& m)
{
  uniforms_.viewToWorld = m;
  maths::invertMatrix(m, &uniforms_.worldToView);

  uniforms_.viewProjection = uniforms_.worldToView * uniforms_.projection;
}

void camera_t::setProjectionMatrix(const maths::mat4& m)
{
  uniforms_.projection = m;
  maths::invertMatrix(m, &uniforms_.projectionInverse);

  uniforms_.viewProjection = uniforms_.worldToView * uniforms_.projection;
}

orbiting_camera_controller_t::orbiting_camera_controller_t()
:target_(0.0f,0.0f,0.0f),
 offset_(0.0f),
 angle_(maths::vec2(0.0f, 0.0f)),
 rotationSensitivity_(0.01f)
{
  Update();
}

orbiting_camera_controller_t::orbiting_camera_controller_t(const maths::vec3& target, const f32 offset, const maths::vec2& angle, f32 rotationSensitivity)
:target_(target), 
 offset_(offset),
 angle_(angle),
 rotationSensitivity_(rotationSensitivity),
 cameraHandle_(bkk::core::BKK_NULL_HANDLE),
 renderer_(nullptr)
{
  Update();
}

void orbiting_camera_controller_t::setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer)
{
  cameraHandle_ = cameraHandle;
  renderer_ = renderer;
  Update();
}

camera_t* orbiting_camera_controller_t::getCamera()
{
  if (renderer_)
    return renderer_->getCamera(cameraHandle_);

  return nullptr;
}

void orbiting_camera_controller_t::Move(f32 amount)
{
  offset_ += amount;
  //offset_ = maths::clamp(0.0f, offset_, offset_);

  Update();
}

void orbiting_camera_controller_t::Rotate(f32 angleY, f32 angleZ)
{
  angle_.x = angle_.x + angleY * rotationSensitivity_;
  angle_.y = angle_.y + angleZ * rotationSensitivity_;
  Update();
}

void orbiting_camera_controller_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.y) *
  maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.x);

  maths::mat4 tx = maths::createTransform(maths::vec3(0.0f, 0.0f, offset_), maths::VEC3_ONE, maths::QUAT_UNIT) * maths::createTransform(maths::VEC3_ZERO, maths::VEC3_ONE, orientation) * maths::createTransform(target_, maths::VEC3_ONE, maths::QUAT_UNIT);  

  if (cameraHandle_ != bkk::core::BKK_NULL_HANDLE)
  {
    camera_t* camera = renderer_->getCamera(cameraHandle_);
    if (camera)
      camera->setViewToWorldMatrix(tx);
  }
  else
  {
    maths::invertMatrix(tx, &view_);
  }
}


free_camera_controller_t::free_camera_controller_t()
:tx_(),
 view_(),
 position_(0.0f, 0.0f, 0.0f),
 angle_(0.0f, 0.0f),
 moveDelta_(1.0f),
 rotationSensitivity_(0.01f),
 cameraHandle_(bkk::core::BKK_NULL_HANDLE),
 renderer_(nullptr)
{
  Update();
}

free_camera_controller_t::free_camera_controller_t(const maths::vec3& position, const maths::vec2& angle, f32 moveDelta, f32 rotationSensitivity)
  :position_(position),
  angle_(angle),
  moveDelta_(moveDelta),
  rotationSensitivity_(rotationSensitivity),
  cameraHandle_(bkk::core::BKK_NULL_HANDLE),
  renderer_(nullptr)
{
  Update();
}

void free_camera_controller_t::setCameraHandle(camera_handle_t cameraHandle, renderer_t* renderer)
{
  cameraHandle_ = cameraHandle;
  renderer_ = renderer;
  Update();
}

camera_t* free_camera_controller_t::getCamera()
{
  if (renderer_)
    return renderer_->getCamera(cameraHandle_);

  return nullptr;
}

void free_camera_controller_t::Move(f32 xAmount, f32 zAmount)
{
  position_ = position_ + (zAmount * tx_.row(2).xyz()) + (xAmount * tx_.row(0).xyz());
  Update();
}

void free_camera_controller_t::Rotate(f32 angleY, f32 angleX)
{
  angle_.y = angle_.y + angleY * rotationSensitivity_;
  angleX = angle_.x + angleX * rotationSensitivity_;
  if (angleX < PI_2 && angleX > -PI_2)
  {
    angle_.x = angleX;
  }

  Update();
}

void free_camera_controller_t::Update()
{
  maths::quat orientation = maths::quaternionFromAxisAngle(maths::vec3(1.0f, 0.0f, 0.0f), angle_.x) * maths::quaternionFromAxisAngle(maths::vec3(0.0f, 1.0f, 0.0f), angle_.y);
  tx_ = maths::createTransform(position_, maths::VEC3_ONE, orientation);  

  if (cameraHandle_ != bkk::core::BKK_NULL_HANDLE)
  {
    camera_t* camera = renderer_->getCamera(cameraHandle_);
    if (camera) 
      camera->setViewToWorldMatrix( tx_ );
  }
  else
  {
    maths::invertMatrix(tx_, &view_);
  }
}

void free_camera_controller_t::onKey(uint32_t key, bool pressed)
{
  if (pressed)
  {
    switch (key)
    {
    case window::key_e::KEY_UP:
    case 'w':
      Move(0.0f, -moveDelta_);
      break;

    case window::key_e::KEY_DOWN:
    case 's':
      Move(0.0f, moveDelta_);
      break;

    case window::key_e::KEY_LEFT:
    case 'a':
      Move(-moveDelta_, 0.0f);
      break;

    case window::key_e::KEY_RIGHT:
    case 'd':
      Move(moveDelta_, 0.0f);
      break;

    default:
      break;
    }
  }
}