/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/application.h"
#include "framework/camera.h"

#include "core/render.h"
#include "core/window.h"
#include "core/image.h"
#include "core/mesh.h"
#include "core/maths.h"
#include "core/timer.h"

using namespace bkk;
using namespace bkk::core;
using namespace bkk::core::maths;

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec3 aNormal;
  layout(location = 2) in vec2 aTextCoord;

  struct particle_t
  {
    vec3 position;
    float scale;    
    vec4 color;
    vec3 angle;
  };

  layout( set = 0, binding = 0) uniform UNIFORMS
  {
    mat4 modelView;
    mat4 modelViewProjection;
  }uniforms;

  layout(set = 0, binding = 1)  readonly buffer PARTICLES
  {
    particle_t data[];
  }particles;  
  
  mat3 rotationFromEuler( vec3 eulerAngles )
  {
    mat3 mx;
	  float s = sin(eulerAngles.x);
	  float c = cos(eulerAngles.x);
	  mx[0] = vec3(c, s, 0.0);
	  mx[1] = vec3(-s, c, 0.0);
	  mx[2] = vec3(0.0, 0.0, 1.0);
	
    mat3 my;
	  s = sin(eulerAngles.y);
	  c = cos(eulerAngles.y);
	  my[0] = vec3(c, 0.0, s);
	  my[1] = vec3(0.0, 1.0, 0.0);
	  my[2] = vec3(-s, 0.0, c);
	
	  mat3 mz;
	  s = sin(eulerAngles.z);
	  c = cos(eulerAngles.z);		
	  mz[0] = vec3(1.0, 0.0, 0.0);
	  mz[1] = vec3(0.0, c, s);
	  mz[2] = vec3(0.0, -s, c);
	
	  return mz * my * mx;
  }

  layout(location = 0) out vec4 color;
  layout(location = 1) out vec2 uv;
  void main(void)
  { 
    color = particles.data[gl_InstanceIndex].color;
    uv = aTextCoord;    
      
    mat3 rotation = rotationFromEuler(particles.data[gl_InstanceIndex].angle);
    vec3 localPosition = aPosition.xyz * rotation * particles.data[gl_InstanceIndex].scale + particles.data[gl_InstanceIndex].position;
    gl_Position = uniforms.modelViewProjection * vec4(localPosition, 1.0);
    
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec4 color;
  layout(location = 1) in vec2 uv;
  layout(location = 0) out vec4 result;
  layout(set = 0, binding = 2) uniform sampler2D particleTexture;
  void main(void)
  {
    vec4 textureColor = texture( particleTexture, uv );
    if( textureColor.a < 0.5 ) discard;
    result = textureColor * color;
  }
)";

static const char* gComputeShader = R"(
  #version 440 core
  #extension GL_ARB_separate_shader_objects : enable
  #extension GL_ARB_shading_language_420pack : enable
  layout (local_size_x = 64, local_size_y = 1) in;

  struct particle_t
  {
    vec3 position;
    float scale;    
    vec4 color;
    vec3 angle;
  };
  layout (std140, binding = 0) buffer SSBO0
  {
    particle_t data[];  
  }particles;

  struct particle_state_t
  {
    vec4 velocity;
    vec4 angularSpeed;
    float age;
  };
  layout (std140, binding = 1) buffer SSBO1
  {
    particle_state_t data[];  
  }particlesState;
  
  layout (std140, binding = 2) buffer SSBO2
  {
    float deltaTime_;
    int particlesToEmit_;    
    float gravity_;
    float particleMaxAge_;
    vec3 emissionVolume_;
    uint maxParticleCount_;
    vec4 emissionDirection_;
    vec2 scale_;
    vec2 initialVelocity_;
    vec3 angularVelocity_;
  }globals;
  
  //Pseudo-random number generation
  uint rng_state = 0;
  void initRand()
  {
    uint seed = gl_GlobalInvocationID.x + uint( 1000.0 * fract( globals.deltaTime_) );
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    rng_state = seed;
  }

  float rand()
  {
    rng_state ^= (rng_state << 13);
    rng_state ^= (rng_state >> 17);
    rng_state ^= (rng_state << 5);
    return float(rng_state) * (1.0 / 4294967296.0);
  }
    
  vec3 RandomPointInSphere()
  {
    float z = rand() * 2.0f - 1.0f;
    float t = rand() * 2.0f * 3.1415926f;
    float r = sqrt(max(0.0, 1.0f - z*z));
    float x = r * cos(t);
    float y = r * sin(t);
    vec3 res = vec3(x,y,z);
    res *= pow(rand(), 1.0/3.0);
    return res;
  }

  void main()
  {    
    initRand();
    uint particleIndex = gl_GlobalInvocationID.x;    
    if( particleIndex > globals.maxParticleCount_ )
    {
      return;      
    }

    if( particlesState.data[particleIndex].age > globals.particleMaxAge_ )
    {
      //Kill particle
      particlesState.data[particleIndex].age = -1.0;
      particles.data[particleIndex].scale = 0.0;
    }

    if( particlesState.data[particleIndex].age < 0 )
    {      
      //Emit if required
      if( atomicAdd( globals.particlesToEmit_, -1 ) > 0 )
      {
        //Initialize particle
        particles.data[particleIndex].scale = mix( globals.scale_.x, globals.scale_.y, rand() );
        vec3 randPos = globals.emissionVolume_ * vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );
        particles.data[particleIndex].position = randPos;
        particles.data[particleIndex].angle = vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );        
        particles.data[particleIndex].color = vec4( rand(), rand(), rand(), 1.0 );

        particlesState.data[particleIndex].age = 0;
        vec3 emissionDirection = normalize(  normalize( globals.emissionDirection_.xyz) + globals.emissionDirection_.w * RandomPointInSphere() );
        particlesState.data[particleIndex].velocity.xyz = mix( globals.initialVelocity_.x, globals.initialVelocity_.y, rand() )  * emissionDirection;
        
      }
      else
      {
        particles.data[particleIndex].scale = 0;
      }
    }
    else
    {
      //Update particle
      particlesState.data[particleIndex].age += globals.deltaTime_;
      particlesState.data[particleIndex].velocity.y -=  globals.gravity_ * globals.deltaTime_;

      particles.data[particleIndex].angle += globals.angularVelocity_.xyz;
      particles.data[particleIndex].position +=  particlesState.data[particleIndex].velocity.xyz * globals.deltaTime_;
    }
  }
)";

class particles_sample_t : public framework::application_t
{
public:

  particles_sample_t()
  :application_t("Particles", 1200u, 800u, 3u),
   camera_(vec3(0.0f,20.0f,0.0f), 50.0f, vec2(0.0f, 0.0f), 0.01f),
   emissionRate_(10000)
  {
    particleSystem_.maxParticleCount = 75000;
    particleSystem_.particleMaxAge = 6.0f;
    particleSystem_.emissionVolume = vec3(0.0f, 0.0f, 0.0f);
    particleSystem_.gravity = 9.8f;
    particleSystem_.emissionDirection = vec4(0.0f,1.0f,0.0f,0.25f );
    particleSystem_.scale = vec2(0.5, 1.0);
    particleSystem_.initialVelocity = vec2(30.0f, 30.0f);
    particleSystem_.angularVelocity = vec3(0.1f, 0.1f, 0.1f);
    
    render::context_t& context = getRenderContext();
    projectionTx_ = perspectiveProjectionMatrix(1.5f, getWindow().width / (float)getWindow().height, 1.0f, 1000.0f);
    modelTx_ = createTransform(vec3(0.0, 0.0, 0.0), VEC3_ONE, QUAT_UNIT);

    //Create uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.getViewMatrix();
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferCreate(context, render::gpu_buffer_t::UNIFORM_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&matrices, sizeof(matrices),
      nullptr, &globalUnifomBuffer_);

    //Create particle mesh and texture
    mesh_ = mesh::unitQuad(context);
    image::image2D_t image = {};
    image::load("../resources/particle.png", false, &image );
    render::texture2DCreate(context, &image, 1u, render::texture_sampler_t(), &particleTexture_);
    image::free(&image);

    //Create particle buffers
    std::vector<particle_t> particles(particleSystem_.maxParticleCount);
    std::vector<particle_state_t> particlesState(particleSystem_.maxParticleCount);
    for (u32 i(0); i < particleSystem_.maxParticleCount; ++i)
    {
      particles[i].scale = 0.0f;
      particlesState[i].age = -1.0f;
    }

    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      (void*)particles.data(), sizeof(particle_t)*particleSystem_.maxParticleCount,
      nullptr, &particleBuffer_);

    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      (void*)particlesState.data(), sizeof(particle_state_t)*particleSystem_.maxParticleCount,
      nullptr, &particleStateBuffer_);

    render::gpuBufferCreate(context, render::gpu_buffer_t::STORAGE_BUFFER,
      (void*)&particleSystem_, sizeof(particle_system_t),
      nullptr, &particleGlobalsBuffer_);
    
    //Create pipeline and descriptor set layouts
    render::descriptor_binding_t bindings[3] = { { render::descriptor_t::type_e::UNIFORM_BUFFER, 0u, render::descriptor_t::stage_e::VERTEX },
                                                 { render::descriptor_t::type_e::STORAGE_BUFFER, 1u, render::descriptor_t::stage_e::VERTEX },
                                                 { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 2u, render::descriptor_t::stage_e::FRAGMENT } };

    render::descriptorSetLayoutCreate(context, bindings, 3u, &descriptorSetLayout_);
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, nullptr, 0u, &pipelineLayout_);

    render::descriptorPoolCreate(context, 2u,
      render::combined_image_sampler_count(1u),
      render::uniform_buffer_count(1u),
      render::storage_buffer_count(4u),
      render::storage_image_count(0u),
      &descriptorPool_);

    //Create descriptor set
    render::descriptor_t descriptors[3] = { render::getDescriptor(globalUnifomBuffer_), render::getDescriptor(particleBuffer_), render::getDescriptor(particleTexture_) };
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, descriptors, &descriptorSet_);

    //Create pipeline
    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);
    render::graphics_pipeline_t::description_t pipelineDesc;
    pipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    pipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    pipelineDesc.blendState.resize(1);
    pipelineDesc.blendState[0].colorWriteMask = 0xF;
    pipelineDesc.blendState[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode = VK_CULL_MODE_NONE;
    pipelineDesc.depthTestEnabled = true;
    pipelineDesc.depthWriteEnabled = true;
    pipelineDesc.depthTestFunction = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader = vertexShader_;
    pipelineDesc.fragmentShader = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain.renderPass, 0u, mesh_.vertexFormat, pipelineLayout_, pipelineDesc, &pipeline_);

    buildCommandBuffers();
    initializeCompute();
  }

  void onQuit()
  {
    render::context_t& context = getRenderContext();

    mesh::destroy(context, &mesh_);
    render::textureDestroy(context, &particleTexture_);

    render::shaderDestroy(context, &vertexShader_);
    render::shaderDestroy(context, &fragmentShader_);

    render::pipelineLayoutDestroy(context, &pipelineLayout_);
    render::graphicsPipelineDestroy(context, &pipeline_);
    render::descriptorSetLayoutDestroy(context, &descriptorSetLayout_);
    render::descriptorSetDestroy(context, &descriptorSet_);    
    render::gpuBufferDestroy(context, nullptr, &globalUnifomBuffer_);
    
    render::gpuBufferDestroy(context, nullptr, &particleGlobalsBuffer_);
    render::gpuBufferDestroy(context, nullptr, &particleBuffer_);
    render::gpuBufferDestroy(context, nullptr, &particleStateBuffer_);
    

    render::shaderDestroy(context, &computeShader_);
    render::descriptorSetDestroy(context, &computeDescriptorSet_);
    render::descriptorSetLayoutDestroy(context, &computeDescriptorSetLayout_);
    render::computePipelineDestroy(context, &computePipeline_);
    render::pipelineLayoutDestroy(context, &computePipelineLayout_);
    render::commandBufferDestroy(context, &computeCommandBuffer_);

    render::descriptorPoolDestroy(context, &descriptorPool_);
  }

  void render()
  {
    render::context_t& context = getRenderContext();

    //Update uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.getViewMatrix();
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferUpdate(context, (void*)&matrices, 0, sizeof(matrices), &globalUnifomBuffer_);
    render::presentFrame(&context);

    particleSystem_.deltaTime = minValue(0.033f, getTimeDelta() / 1000.0f);
    static float particlesToEmit = 0.0f;
    particleSystem_.particlesToEmit = 0;
    particlesToEmit += emissionRate_ * particleSystem_.deltaTime;
    if (particlesToEmit > 1.0f)
    {
      particleSystem_.particlesToEmit = (s32)particlesToEmit;
      particlesToEmit = 0.0f;
    }

    render::gpuBufferUpdate(context, (void*)&particleSystem_, 0u, 8u, &particleGlobalsBuffer_);
    render::commandBufferSubmit(context, computeCommandBuffer_);
    //vkQueueWaitIdle(context.computeQueue_.handle);
  }

  void onResize(u32 width, u32 height)
  {
    buildCommandBuffers();
    projectionTx_ = perspectiveProjectionMatrix(1.5f, width / (float)height, 1.0f, 1000.0f);
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
        camera_.move(-1.0f);
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        camera_.move(1.0f);
        break;
      }

      case 'r':
      {
        render::context_t& context = getRenderContext();
        render::contextFlush(context);
        std::vector<particle_state_t> particlesState(particleSystem_.maxParticleCount);
        for (u32 i(0); i < particleSystem_.maxParticleCount; ++i)
        {
          particlesState[i].age = -1.0f;
        }
        render::gpuBufferUpdate(context, particlesState.data(), 0u, sizeof(particle_state_t)* particleSystem_.maxParticleCount, &particleStateBuffer_);
        break;
      }
      default:
        break;
      }
    }
  }

  void onMouseMove(const vec2& mousePos, const vec2& mouseDeltaPos)
  {
    if (getMousePressedButton() > -1)
    {
      camera_.rotate(mouseDeltaPos.x, mouseDeltaPos.y);
    }
  }

  void buildCommandBuffers()
  {
    render::context_t& context = getRenderContext();

    VkClearValue clearValues[2];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    clearValues[1].depthStencil = { 1.0f,0 };
    const render::command_buffer_t* commandBuffers;
    uint32_t count = render::getPresentationCommandBuffers(context, &commandBuffers);
    for (uint32_t i(0); i<count; ++i)
    {
      render::beginPresentationCommandBuffer(context, i, clearValues);
      render::graphicsPipelineBind(commandBuffers[i], pipeline_);
      render::descriptorSetBind(commandBuffers[i], pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::drawInstanced(commandBuffers[i], particleSystem_.maxParticleCount, nullptr, 0u, mesh_);
      render::endPresentationCommandBuffer(context, i);
    }
  }

  void initializeCompute()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t bindings[3] = { { render::descriptor_t::type_e::STORAGE_BUFFER, 0, render::descriptor_t::stage_e::COMPUTE },
                                                 { render::descriptor_t::type_e::STORAGE_BUFFER, 1, render::descriptor_t::stage_e::COMPUTE },
                                                 { render::descriptor_t::type_e::STORAGE_BUFFER, 2, render::descriptor_t::stage_e::COMPUTE } };

    render::descriptorSetLayoutCreate(context, bindings, 3u, &computeDescriptorSetLayout_);
    render::pipelineLayoutCreate(context, &computeDescriptorSetLayout_, 1u, nullptr, 0u, &computePipelineLayout_);

    //Create descriptor set
    render::descriptor_t descriptors[3] = { render::getDescriptor(particleBuffer_), render::getDescriptor(particleStateBuffer_), render::getDescriptor(particleGlobalsBuffer_) };
    render::descriptorSetCreate(context, descriptorPool_, computeDescriptorSetLayout_, descriptors, &computeDescriptorSet_);

    //Create pipeline
    render::shaderCreateFromGLSLSource(context, render::shader_t::COMPUTE_SHADER, gComputeShader, &computeShader_);
    render::computePipelineCreate(context, computePipelineLayout_, computeShader_, &computePipeline_);

    //Build compute command buffer
    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, VK_NULL_HANDLE, &computeCommandBuffer_);
    render::commandBufferBegin(context, computeCommandBuffer_);
    render::computePipelineBind(computeCommandBuffer_, computePipeline_);
    render::descriptorSetBind(computeCommandBuffer_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);
    u32 groupSizeX = (particleSystem_.maxParticleCount + 63) / 64;
    render::computeDispatch(computeCommandBuffer_, groupSizeX, 1, 1);
    render::commandBufferEnd(computeCommandBuffer_);
  }

private:

  struct particle_system_t
  {
    f32 deltaTime;
    s32 particlesToEmit;
    f32 gravity;
    f32 particleMaxAge;
    vec3 emissionVolume;
    u32 maxParticleCount;
    vec4 emissionDirection;  //Direction (in local space) and cone angle
    vec2 scale;
    vec2 initialVelocity;
    vec3 angularVelocity;
    f32 padding;
  };

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
    vec4 velocity;
    vec4 angularSpeed;
    float age;
    vec3 padding;
  };

  particle_system_t particleSystem_;
  
  render::descriptor_pool_t descriptorPool_;

  render::descriptor_set_t descriptorSet_;
  render::graphics_pipeline_t pipeline_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;
  render::gpu_buffer_t globalUnifomBuffer_;
  mesh::mesh_t mesh_;
  render::texture_t particleTexture_;
  render::vertex_format_t vertexFormat_;
  render::pipeline_layout_t pipelineLayout_;
  render::descriptor_set_layout_t descriptorSetLayout_;

  render::gpu_buffer_t particleGlobalsBuffer_;
  render::gpu_buffer_t particleBuffer_;
  render::gpu_buffer_t particleStateBuffer_;
  render::pipeline_layout_t computePipelineLayout_;
  render::descriptor_set_layout_t computeDescriptorSetLayout_;
  render::descriptor_set_t computeDescriptorSet_;
  render::compute_pipeline_t computePipeline_;
  render::command_buffer_t computeCommandBuffer_;
  render::shader_t computeShader_;

  framework::orbiting_camera_controller_t camera_;
  maths::mat4 projectionTx_;
  maths::mat4 modelTx_;
  f32 emissionRate_ = 100.0f;
};

//Entry point
int main()
{
  particles_sample_t sample;
  sample.run();
  return 0;
}
