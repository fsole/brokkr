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

#include "application.h"
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "timer.h"
#include "camera.h"
#include "gui.h"

using namespace bkk;
using namespace maths;

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec3 aNormal;

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
  void main(void)
  { 
    color = particles.data[gl_InstanceIndex].color;      
    mat3 rotation = rotationFromEuler(particles.data[gl_InstanceIndex].angle);
    vec3 localPosition = aPosition.xyz * rotation * particles.data[gl_InstanceIndex].scale + particles.data[gl_InstanceIndex].position;
    gl_Position = uniforms.modelViewProjection * vec4(localPosition, 1.0);
    
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core
  layout(location = 0) in vec4 color;
  layout(location = 0) out vec4 result;
  void main(void)
  {
    result = color;
  }
)";

//Compute density and pressure of each particle
static const char* gComputeDensityShaderSource = R"(
  #version 440 core
  #extension GL_ARB_separate_shader_objects : enable
  #extension GL_ARB_shading_language_420pack : enable
  layout (local_size_x = 64, local_size_y = 1) in;
  const float PI = 3.1415926;

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
    vec3 velocity;
    float age;
    float density;
    float pressure;
    float mass;
  };

  layout (std140, binding = 1) buffer SSBO1
  {
    particle_state_t data[];  
  }particlesState;
  
  layout (std140, binding = 2) buffer SSBO2
  {
    float deltaTime;
    int particlesToEmit;
    float gravity;
    float particleMaxAge;
    vec3 emissionVolume;
    uint maxParticleCount;
    vec4 emissionDirection;
    vec2 scale;
    vec2 initialVelocity;
    vec4 boundaries[6];    
    float viscosityCoefficient;
    float pressureCoefficient;
    float smoothingRadius;
    float referenceDensity;
    vec2 particleMassRange;
  }globals;
  
  void main()
  {    
    float h = globals.smoothingRadius; //smoothing radius parameter
    float h9 = pow(h, 9);
    float h2 = h*h;
    float poly6Coefficient = (315.0f / (64.0f * PI * h9));
    

    uint particleIndex = gl_GlobalInvocationID.x;    
    if( particleIndex > globals.maxParticleCount )
    {
      return;      
    }
    
    particlesState.data[particleIndex].density = 0;
    for( int i = 0; i<globals.maxParticleCount; ++i )
    {
      if( particlesState.data[i].age != -1 )
      {
        vec3 diff = particles.data[particleIndex].position - particles.data[i].position;
        float r2 = dot(diff, diff);
        if(r2 < h2)
        {
          const float W = poly6Coefficient * pow(h2 - r2, 3);
          particlesState.data[particleIndex].density += particlesState.data[particleIndex].mass * W;
        }
      }
    }

    particlesState.data[particleIndex].density = max(globals.referenceDensity, particlesState.data[particleIndex].density);
    particlesState.data[particleIndex].pressure = globals.pressureCoefficient * (particlesState.data[particleIndex].density - globals.referenceDensity);
  }
)";

//Updates position and velocity of each particle
static const char* gUpdateParticlesShaderSource = R"(
  #version 440 core
  #extension GL_ARB_separate_shader_objects : enable
  #extension GL_ARB_shading_language_420pack : enable
  layout (local_size_x = 64, local_size_y = 1) in;

  const float PI = 3.1415926;
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
    vec3 velocity;    
    float age;
    float density;
    float pressure;
    float mass;
  };
  layout (std140, binding = 1) buffer SSBO1
  {
    particle_state_t data[];  
  }particlesState;
  
  layout (std140, binding = 2) buffer SSBO2
  {
    float deltaTime;
    int particlesToEmit;    
    float gravity;
    float particleMaxAge;
    vec3 emissionVolume;
    uint maxParticleCount;
    vec4 emissionDirection;
    vec2 scale;
    vec2 initialVelocity;
    vec4 boundaries[6];    
    float viscosityCoefficient;
    float pressureCoefficient;
    float smoothingRadius;
    float referenceDensity;
    vec2 particleMassRange;
  }globals;
  
  //Pseudo-random number generation
  uint rng_state = 0;
  void initRand()
  {
    uint seed = gl_GlobalInvocationID.x + uint( 1000.0 * fract( globals.deltaTime) );
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
    float t = rand() * 2.0f * PI;
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
    if( particleIndex > globals.maxParticleCount )
    {
      return;      
    }

    if( particlesState.data[particleIndex].age < 0 )
    {      
      //Emit if required
      if( atomicAdd( globals.particlesToEmit, -1 ) > 0 )
      {
        //Initialize particle
        particles.data[particleIndex].scale = mix( globals.scale.x, globals.scale.y, rand() );
        vec3 randPos = globals.emissionVolume * vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );
        particles.data[particleIndex].position = randPos;
        particles.data[particleIndex].angle = vec3( 2.0*rand() - 1.0,2.0*rand() - 1.0,2.0*rand() - 1.0 );        
        particles.data[particleIndex].color = vec4( rand(), rand(), rand(), 1.0 );

        particlesState.data[particleIndex].age = 0;
        vec3 emissionDirection = normalize(  normalize( globals.emissionDirection.xyz) + globals.emissionDirection.w * RandomPointInSphere() );
        particlesState.data[particleIndex].velocity = mix( globals.initialVelocity.x, globals.initialVelocity.y, rand() ) * emissionDirection;
        particlesState.data[particleIndex].mass = mix( globals.particleMassRange.x, globals.particleMassRange.y, rand() );
      }
      else
      {
        particles.data[particleIndex].scale = 0;
      }
    }
    else
    { 
      vec3 acceleration = vec3(0,0,0);

      float h = globals.smoothingRadius;
      float h2 = h*h;
      float h3 = h*h*h;      
      float spikyGradCoefficient = (-45 / (PI * pow(h,6.0)));
      float viscosityLaplacianCoefficient = 45.0/(PI*pow(h, 6.0f));
      float particlePressure = particlesState.data[particleIndex].pressure;
      float particleDensity = particlesState.data[particleIndex].density;

      if( particlesState.data[particleIndex].age > 1.0 )
      {
        for( int i = 0; i<globals.maxParticleCount; ++i )
        {
          if( particlesState.data[i].age != -1 && i != particleIndex)
          {          
            vec3 r = particles.data[particleIndex].position - particles.data[i].position;
            float distance = length( r );
            if( distance < h )
            {
              //Acceleration due to pressure
              float diff = h - distance;
              float spiky = spikyGradCoefficient*diff*diff;
              float massRatio = particlesState.data[i].mass / particlesState.data[particleIndex].mass;
              float pterm = (particlePressure + particlesState.data[i].pressure) / (2*particleDensity*particlesState.data[i].density);
              acceleration -= massRatio*pterm*spiky*r;
              
              //Acceleration due to viscosity
              float lap = viscosityLaplacianCoefficient*diff;
              vec3 vdiff = particlesState.data[i].velocity - particlesState.data[particleIndex].velocity;
              acceleration += globals.viscosityCoefficient*massRatio*(1/particlesState.data[i].density)*lap*vdiff;
            }
          }
        }
      }

      //Acceleration due to gravity
      acceleration.y -= 9.8;

      //Update particle position, velocity and age
      particlesState.data[particleIndex].age += globals.deltaTime;
      particlesState.data[particleIndex].velocity +=  acceleration * globals.deltaTime;
      particles.data[particleIndex].position +=  particlesState.data[particleIndex].velocity * globals.deltaTime;
      
      //Bounds check
      for( int i=0; i<6; ++i )
      { 
        vec4 bound = globals.boundaries[i];
        bound.xyz = normalize( bound.xyz );
        float distanceToPlane = dot( bound, vec4(particles.data[particleIndex].position, 1.0) );
        if( distanceToPlane <= 0.0 )
        {
          particles.data[particleIndex].position -= bound.xyz*distanceToPlane;
          vec3 reflectionDirection = reflect( particlesState.data[particleIndex].velocity, bound.xyz );
          particlesState.data[particleIndex].velocity = reflectionDirection * 0.3;
        }
      }
    }  
  }
)";

class fluid_simulation_sample_t : public application_t
{
public:

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
    vec4 boundaries[6];    
    f32 viscosityCoefficient;
    f32 pressureCoefficient;
    f32 smoothingRadius;
    f32 referenceDensity;
    vec2 particleMassRange;
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
    vec3 velocity;
    f32 age;
    f32 density;
    f32 pressure;
    f32 mass;
    f32 padding;
  };

  fluid_simulation_sample_t()
    :application_t("SPH Fluid Simulation", 1200u, 800u, 3u),
    camera_(vec3(0.0f, -40.0f, 0.0f), 35.0f, vec2(0.0f, 0.0f), 0.01f),
    emissionRate_(500)
  {
    particleSystem_.maxParticleCount = 5000;
    particleSystem_.particleMaxAge = 6.0f;
    particleSystem_.emissionVolume = vec3(0.5f, 0.5f, 0.5f);
    particleSystem_.gravity = 9.8f;
    particleSystem_.emissionDirection = vec4(0.0f, -1.0f, 0.0f, 0.05f);
    particleSystem_.scale = vec2(0.5f, 0.5f);
    particleSystem_.initialVelocity = vec2(30.0f,30.0f);
    particleSystem_.particleMassRange = vec2(1.5f, 1.5f);
    particleSystem_.viscosityCoefficient = 1.5f;
    particleSystem_.pressureCoefficient = 250.0f;
    particleSystem_.smoothingRadius = 1.0f;
    particleSystem_.referenceDensity = 1.5f;
    particleSystem_.boundaries[0] = vec4( 0.0f,  1.0f,  0.0f,  55.0f);
    particleSystem_.boundaries[1] = vec4( 0.0f, -1.0f,  0.0f,  25.0f);
    particleSystem_.boundaries[2] = vec4( 1.0f,  0.0f,  0.0f,  15.0f);
    particleSystem_.boundaries[3] = vec4(-1.0f,  0.0f,  0.0f,  15.0f);
    particleSystem_.boundaries[4] = vec4( 0.0f,  0.0f,  1.0f,  15.0f);
    particleSystem_.boundaries[5] = vec4( 0.0f,  0.0f, -1.0f,  15.0f);

    render::context_t& context = getRenderContext();
    projectionTx_ = perspectiveProjectionMatrix(1.5f, getWindow().width_ / (float)getWindow().height_, 1.0f, 1000.0f);
    modelTx_ = createTransform(vec3(0.0, 0.0, 0.0), VEC3_ONE, QUAT_UNIT);

    //Create particle mesh
    mesh::createFromFile(context, "../resources/sphere.obj", mesh::EXPORT_ALL, nullptr, 0u, &mesh_);

    //Create uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferCreate(context, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&matrices, sizeof(matrices),
      nullptr, &globalUnifomBuffer_);

    //Create particle buffers
    std::vector<particle_t> particles(particleSystem_.maxParticleCount);
    std::vector<particle_state_t> particlesState(particleSystem_.maxParticleCount);
    for (u32 i(0); i < particleSystem_.maxParticleCount; ++i)
    {
      particles[i].scale = 0.0f;
      particlesState[i].age = -1.0f;
      particlesState[i].density = 0.0f;
    }

    u32 usage = render::gpu_buffer_t::usage::STORAGE_BUFFER;
    render::gpuBufferCreate(context, usage,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)particles.data(), sizeof(particle_t)*particleSystem_.maxParticleCount,
      nullptr, &particleBuffer_);

    render::gpuBufferCreate(context, usage,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)particlesState.data(), sizeof(particle_state_t)*particleSystem_.maxParticleCount,
      nullptr, &particleStateBuffer_);

    render::gpuBufferCreate(context, usage,
      render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
      (void*)&particleSystem_, sizeof(particle_system_t),
      nullptr, &particleGlobalsBuffer_);

    //Create pipeline and descriptor set layouts
    render::descriptor_binding_t bindings[2] = { { render::descriptor_t::type::UNIFORM_BUFFER, 0u, render::descriptor_t::stage::VERTEX },
                                                 { render::descriptor_t::type::STORAGE_BUFFER, 1u, render::descriptor_t::stage::VERTEX } };

    render::descriptorSetLayoutCreate(context, bindings, 2u, &descriptorSetLayout_);
    render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, nullptr, 0u, &pipelineLayout_);

    render::descriptorPoolCreate(context, 2u,
      render::combined_image_sampler_count(0u),
      render::uniform_buffer_count(1u),
      render::storage_buffer_count(3u),
      render::storage_image_count(0u),
      &descriptorPool_);

    //Create descriptor set
    render::descriptor_t descriptors[2] = { render::getDescriptor(globalUnifomBuffer_), render::getDescriptor(particleBuffer_)};
    render::descriptorSetCreate(context, descriptorPool_, descriptorSetLayout_, descriptors, &descriptorSet_);

    //Create pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader_);
    bkk::render::graphics_pipeline_t::description_t pipelineDesc;
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
    pipelineDesc.cullMode_ = VK_CULL_MODE_NONE;
    pipelineDesc.depthTestEnabled_ = true;
    pipelineDesc.depthWriteEnabled_ = true;
    pipelineDesc.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDesc.vertexShader_ = vertexShader_;
    pipelineDesc.fragmentShader_ = fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh_.vertexFormat_, pipelineLayout_, pipelineDesc, &pipeline_);

    initializeCompute();
  }

  void onQuit()
  {
    render::context_t& context = getRenderContext();
    render::contextFlush(context);

    mesh::destroy(context, &mesh_);

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

    render::descriptorSetDestroy(context, &computeDescriptorSet_);
    render::descriptorSetLayoutDestroy(context, &computeDescriptorSetLayout_);
    render::pipelineLayoutDestroy(context, &computePipelineLayout_);

    render::shaderDestroy(context, &updateParticlesShader_);
    render::computePipelineDestroy(context, &updateParticlesComputePipeline_);
    render::commandBufferDestroy(context, &updateParticlesCommandBuffer_);

    render::shaderDestroy(context, &computeDensityShader_);
    render::computePipelineDestroy(context, &computeDensityPipeline_);
    render::commandBufferDestroy(context, &computeDensityCommandBuffer_);

    render::descriptorPoolDestroy(context, &descriptorPool_);
  }

  void render()
  {
    render::context_t& context = getRenderContext();

    //Update uniform buffer
    mat4 matrices[2];
    matrices[0] = modelTx_ * camera_.view_;
    matrices[1] = matrices[0] * projectionTx_;
    render::gpuBufferUpdate(context, (void*)&matrices, 0, sizeof(matrices), &globalUnifomBuffer_);
    buildCommandBuffers();
    render::presentFrame(&context);

    particleSystem_.deltaTime = min( 0.033f, getTimeDelta() / 1000.0f );
    static float particlesToEmit = 0.0f;
    particleSystem_.particlesToEmit = 0;
    particlesToEmit += emissionRate_ * particleSystem_.deltaTime;
    if (particlesToEmit > 1.0f)
    {
      particleSystem_.particlesToEmit = (s32)particlesToEmit;
      particlesToEmit = 0.0f;
    }

    render::gpuBufferUpdate(context, (void*)&particleSystem_, 0u, 8u, &particleGlobalsBuffer_);
    render::commandBufferSubmit(context, computeDensityCommandBuffer_);
    render::commandBufferSubmit(context, updateParticlesCommandBuffer_);
  }

  void onResize(u32 width, u32 height)
  {
    buildCommandBuffers();
    projectionTx_ = perspectiveProjectionMatrix(1.5f, width / (float)height, 1.0f, 1000.0f);
  }

  void restartSimulation()
  {
    render::context_t& context = getRenderContext();
    render::contextFlush(context);
    std::vector<particle_state_t> particlesState(particleSystem_.maxParticleCount);
    for (u32 i(0); i < particleSystem_.maxParticleCount; ++i)
    {
      particlesState[i].age = -1.0f;
      particlesState[i].density = 0.0f;
    }
    render::gpuBufferUpdate(context, particlesState.data(), 0u, sizeof(particle_state_t)* particleSystem_.maxParticleCount, &particleStateBuffer_);
    render::gpuBufferUpdate(context, (void*)&particleSystem_, 0u, sizeof(particle_system_t), &particleGlobalsBuffer_);
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
        camera_.Move(-1.0f);
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        camera_.Move(1.0f);
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
    {
      camera_.Rotate(mouseDeltaPos.x, mouseDeltaPos.y);
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
      bkk::render::graphicsPipelineBind(commandBuffers[i], pipeline_);
      bkk::render::descriptorSetBind(commandBuffers[i], pipelineLayout_, 0, &descriptorSet_, 1u);
      mesh::drawInstanced(commandBuffers[i], particleSystem_.maxParticleCount, nullptr, 0u, mesh_);

      gui::draw(context, commandBuffers[i]);
      render::endPresentationCommandBuffer(context, i);
    }
  }

  void initializeCompute()
  {
    render::context_t& context = getRenderContext();

    //Create descriptor layout
    render::descriptor_binding_t bindings[3] = { { render::descriptor_t::type::STORAGE_BUFFER, 0, render::descriptor_t::stage::COMPUTE },
                                                 { render::descriptor_t::type::STORAGE_BUFFER, 1, render::descriptor_t::stage::COMPUTE },
                                                 { render::descriptor_t::type::STORAGE_BUFFER, 2, render::descriptor_t::stage::COMPUTE } };

    render::descriptorSetLayoutCreate(context, bindings, 3u, &computeDescriptorSetLayout_);
    render::pipelineLayoutCreate(context, &computeDescriptorSetLayout_, 1u, nullptr, 0u, &computePipelineLayout_);

    //Create descriptor set (used by both compute pipelines)
    render::descriptor_t descriptors[3] = { render::getDescriptor(particleBuffer_), render::getDescriptor(particleStateBuffer_), render::getDescriptor(particleGlobalsBuffer_) };
    render::descriptorSetCreate(context, descriptorPool_, computeDescriptorSetLayout_, descriptors, &computeDescriptorSet_);

    u32 groupSizeX = (particleSystem_.maxParticleCount + 63) / 64;

    //Create computeDensity pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::COMPUTE_SHADER, gComputeDensityShaderSource, &computeDensityShader_);
    render::computePipelineCreate(context, computePipelineLayout_, computeDensityShader_, &computeDensityPipeline_);

    //Build computeDensity command buffer
    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, &computeDensityCommandBuffer_);
    render::commandBufferBegin(context, computeDensityCommandBuffer_);
    bkk::render::computePipelineBind(computeDensityCommandBuffer_, computeDensityPipeline_);
    bkk::render::descriptorSetBind(computeDensityCommandBuffer_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);
    bkk::render::computeDispatch(computeDensityCommandBuffer_, groupSizeX, 1u, 1u);
    render::commandBufferEnd(computeDensityCommandBuffer_);

    //Create updateParticle pipeline
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::COMPUTE_SHADER, gUpdateParticlesShaderSource, &updateParticlesShader_);
    render::computePipelineCreate(context, computePipelineLayout_, updateParticlesShader_, &updateParticlesComputePipeline_);

    //Build updateParticle command buffer
    render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, &updateParticlesCommandBuffer_);
    render::commandBufferBegin(context, updateParticlesCommandBuffer_);
    bkk::render::computePipelineBind(updateParticlesCommandBuffer_, updateParticlesComputePipeline_);
    bkk::render::descriptorSetBind(updateParticlesCommandBuffer_, computePipelineLayout_, 0, &computeDescriptorSet_, 1u);    
    bkk::render::computeDispatch(updateParticlesCommandBuffer_, groupSizeX, 1u, 1u);
    render::commandBufferEnd(updateParticlesCommandBuffer_);
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::SliderFloat("viscosity", &particleSystem_.viscosityCoefficient, 0.0f, 10.0f);
    ImGui::SliderFloat("pressure", &particleSystem_.pressureCoefficient, 0.0f, 500.0f);
    ImGui::SliderFloat("referenceDensity", &particleSystem_.referenceDensity, 0.0f, 10.0f);
    if (ImGui::Button("Reset")){ restartSimulation(); }
    ImGui::End();
  }

private:

  particle_system_t particleSystem_;
  render::descriptor_pool_t descriptorPool_;
  render::descriptor_set_t descriptorSet_;
  render::graphics_pipeline_t pipeline_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;
  render::gpu_buffer_t globalUnifomBuffer_;
  mesh::mesh_t mesh_;
  render::vertex_format_t vertexFormat_;
  render::pipeline_layout_t pipelineLayout_;
  render::descriptor_set_layout_t descriptorSetLayout_;

  render::gpu_buffer_t particleGlobalsBuffer_;
  render::gpu_buffer_t particleBuffer_;
  render::gpu_buffer_t particleStateBuffer_;
  render::pipeline_layout_t computePipelineLayout_;
  render::descriptor_set_layout_t computeDescriptorSetLayout_;
  render::descriptor_set_t computeDescriptorSet_;
  
  render::compute_pipeline_t updateParticlesComputePipeline_;
  render::command_buffer_t updateParticlesCommandBuffer_;
  render::shader_t updateParticlesShader_;

  render::compute_pipeline_t computeDensityPipeline_;
  render::command_buffer_t computeDensityCommandBuffer_;
  render::shader_t computeDensityShader_;

  camera::orbiting_camera_t camera_;
  maths::mat4 projectionTx_;
  maths::mat4 modelTx_;
  f32 emissionRate_ = 100.0f;

  vec4 boudaries[6];
};

//Entry point
int main()
{
  fluid_simulation_sample_t sample;
  sample.loop();
  return 0;
}
