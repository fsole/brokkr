/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/mesh.h"
#include "core/maths.h"
#include "core/image.h"

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

using namespace bkk::core;
using namespace bkk::framework;
using namespace bkk::core::maths;

class fluid_simulation_sample_t : public application_t
{
public:

  fluid_simulation_sample_t()
  :application_t("SPH Fluid Simulation", 1200u, 800u, 3u),
  cameraController_(vec3(0.0f, -10.0f, 0.0f), 45.0f, vec2(-0.8f, 0.0f), 0.01f)
  { 
    //Create particle buffers
    std::vector<particle_t> particles(maxParticleCount_);
    std::vector<particle_state_t> particlesState(maxParticleCount_);
    for (u32 i(0); i < maxParticleCount_; ++i)
    {
      particles[i].scale = 0.0f;
      particlesState[i].age = -1.0f;
      particlesState[i].density = 0.0f;
    }

    render::context_t& context = getRenderContext();
    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      (void*)particles.data(), sizeof(particle_t)*maxParticleCount_,
      nullptr, &particleBuffer_);

    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      (void*)particlesState.data(), sizeof(particle_state_t)*maxParticleCount_,
      nullptr, &particleStateBuffer_);

    //Create and configure compute material
    renderer_t& renderer = getRenderer();
    shader_handle_t computeShader = renderer.shaderCreate("../fluid-simulation/fluid-simulation.shader");
    computeMaterial_ = renderer.computeMaterialCreate(computeShader);
    compute_material_t* computePtr = renderer.getComputeMaterial(computeMaterial_);    
    computePtr->setProperty("globals.gravity", gravity_);
    computePtr->setProperty("globals.viscosityCoefficient", viscosityCoefficient_);
    computePtr->setProperty("globals.pressureCoefficient", pressureCoefficient_);
    computePtr->setProperty("globals.referenceDensity", referenceDensity_);
    computePtr->setProperty("globals.maxParticleCount", maxParticleCount_);    
    computePtr->setProperty("globals.emissionVolume", vec3(0.5f, 0.5f, 0.5f) );    
    computePtr->setProperty("globals.emissionDirection", vec4(0.0f, -1.0f, 0.0f, 0.05f) );
    computePtr->setProperty("globals.initialVelocity", 30.0f);
    computePtr->setProperty("globals.particleMass", 1.5f );
    computePtr->setProperty("globals.smoothingRadius", 1.0f );
    computePtr->setProperty("globals.boundaries", &boundaries_ );
    computePtr->setBuffer("particles", particleBuffer_);
    computePtr->setBuffer("particlesState", particleStateBuffer_);

    //Create particle actor
    mesh_handle_t particleMesh = renderer.meshCreate("../resources/sphere.obj", mesh::EXPORT_ALL);
    shader_handle_t shader = renderer.shaderCreate("../fluid-simulation/particles.shader");
    material_handle_t particleMaterial = renderer.materialCreate(shader);
    material_t* particleMaterialPtr = renderer.getMaterial(particleMaterial);
    particleMaterialPtr->setBuffer("particles", particleBuffer_);
    renderer.actorCreate("particles", particleMesh, particleMaterial, mat4(), maxParticleCount_);

    //create camera
    uvec2 imageSize(1200u, 800u);
    camera_ = renderer.addCamera(camera_t(camera_t::PERSPECTIVE_PROJECTION, 1.2f, imageSize.x / (float)imageSize.y, 0.1f, 500.0f));
    cameraController_.setCameraHandle(camera_, &renderer);
  }

  void onQuit()
  {
    render::gpuBufferDestroy(getRenderContext(), nullptr, &particleBuffer_);
    render::gpuBufferDestroy(getRenderContext(), nullptr, &particleStateBuffer_);
  }

  void render()
  {
    beginFrame();

    //Update simulation computeMaterial
    renderer_t& renderer = getRenderer();
    compute_material_t* computePtr = renderer.getComputeMaterial(computeMaterial_);
    computePtr->setProperty("globals.gravity", gravity_);
    computePtr->setProperty("globals.viscosityCoefficient", viscosityCoefficient_);
    computePtr->setProperty("globals.pressureCoefficient", pressureCoefficient_);
    computePtr->setProperty("globals.referenceDensity", referenceDensity_);

    f32 deltaTime = minValue(0.033f, getTimeDelta() / 1000.0f);
    computePtr->setProperty("globals.deltaTime", deltaTime);

    //Determine number of particles that need to be emmitted next frame
    static float particlesToEmit = 0.0f;
    particlesToEmit += emissionRate_ * deltaTime;
    computePtr->setProperty("globals.particlesToEmit", (uint32_t)particlesToEmit);
    if (particlesToEmit > 0.0f) particlesToEmit -= (uint32_t)particlesToEmit;

    //Run simulation compute shaders
    u32 groupSizeX = (maxParticleCount_ + 63) / 64;
    command_buffer_t computeDensity(&renderer, command_buffer_t::COMPUTE);
    computeDensity.dispatchCompute(computeMaterial_, "computeDensity", groupSizeX, 1u, 1u);
    computeDensity.submit();
    computeDensity.release();

    command_buffer_t updateParticles(&renderer, command_buffer_t::COMPUTE, &computeDensity);
    updateParticles.dispatchCompute(computeMaterial_, "updateParticles", groupSizeX, 1u, 1u);
    updateParticles.submit();
    updateParticles.release();

    //Render particles
    renderer.setupCamera(camera_);
    actor_t* visibleActors = nullptr;
    int count = renderer.getVisibleActors(camera_, &visibleActors);
    command_buffer_t renderSceneCmd(&renderer, command_buffer_t::GRAPHICS, NULL_HANDLE, &updateParticles);
    renderSceneCmd.clearRenderTargets(vec4(0.0f, 0.0f, 0.0f, 1.0f));
    renderSceneCmd.render(visibleActors, count, "OpaquePass");
    renderSceneCmd.submit();
    renderSceneCmd.release();

    presentFrame();
  }
  
  void restartSimulation()
  {
    render::context_t& context = getRenderContext();
    render::contextFlush(context);
    std::vector<particle_state_t> particlesState(maxParticleCount_);
    for (u32 i(0); i < maxParticleCount_; ++i)
    {
      particlesState[i].age = -1.0f;
      particlesState[i].density = 0.0f;
    }
    render::gpuBufferUpdate(context, particlesState.data(), 0u, sizeof(particle_state_t)* maxParticleCount_, &particleStateBuffer_);
  }

  void onKeyEvent(u32 key, bool pressed)
  {
    if (pressed)
    {
      switch (key)
      {
      case window::key_e::KEY_UP:
      case 'w':
      {
        cameraController_.Move(-1.0f);
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        cameraController_.Move(1.0f);
        break;
      }      
      case 'r':
      {
        restartSimulation();
        break;
      }
      default:
        break;
      }
    }
  }

  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos)
  {
    if (getMousePressedButton() == window::MOUSE_RIGHT)
      cameraController_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);    
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");

    ImGui::SliderFloat("gravity", &gravity_, -20.0f, 20.0f);
    ImGui::SliderFloat("viscosity", &viscosityCoefficient_, 0.0f, 10.0f);
    ImGui::SliderFloat("pressure", &pressureCoefficient_, 0.0f, 500.0f);
    ImGui::SliderFloat("referenceDensity", &referenceDensity_, 0.0f, 10.0f);
    ImGui::SliderFloat("emissionRate", &emissionRate_, 0.0f, 1000.0f);
    if (ImGui::Button("Reset")){ restartSimulation(); }

    ImGui::End();
  }

private:

  struct particle_t
  {
    vec3 position;
    f32 scale;
    vec4 color;
    vec3 angle;
    f32 padding;
  };

  struct particle_state_t
  {
    vec3 velocity;
    f32 age;
    f32 density;
    f32 pressure;
    f32 mass;
    f32 padding;
  };

  compute_material_handle_t computeMaterial_;    
  render::gpu_buffer_t particleBuffer_;
  render::gpu_buffer_t particleStateBuffer_;

  camera_handle_t camera_;
  orbiting_camera_controller_t cameraController_;

  //Simulation parameters
  f32 gravity_ = 9.8f;
  f32 pressureCoefficient_ = 250.0f;
  f32 referenceDensity_ = 1.5f;
  f32 viscosityCoefficient_ = 1.5f;
  f32 emissionRate_ = 500.0f;
  uint32_t maxParticleCount_ = 5000u;
  vec4 boundaries_[6] = { vec4(0.0f, 1.0f, 0.0f, 25.0f), vec4(0.0f, -1.0f, 0.0f, 25.0f),
                          vec4(1.0f, 0.0f, 0.0f, 15.0f), vec4(-1.0f, 0.0f, 0.0f, 15.0f),
                          vec4(0.0f, 0.0f, 1.0f, 15.0f), vec4(0.0f, 0.0f, -1.0f, 15.0f) };
};

int main()
{
  fluid_simulation_sample_t sample;
  sample.loop();
  return 0;
}
