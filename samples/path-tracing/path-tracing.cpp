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

#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "../utility.h"
#include <cstddef>
#include <cstdlib>
#include <time.h>
#include <math.h>
#include <array>

using namespace bkk;

static render::context_t gContext;
static window::window_t gWindow;
static render::texture_t gTexture;
static mesh::mesh_t gMesh;

static render::descriptor_pool_t gDescriptorPool;

static render::pipeline_layout_t gPipelineLayout;
static render::descriptor_set_t gDescriptorSet;
static render::graphics_pipeline_t gPipeline;


static render::pipeline_layout_t gComputePipelineLayout;
static render::descriptor_set_t gComputeDescriptorSet;
static render::compute_pipeline_t gComputePipeline;
static render::gpu_buffer_t gUbo;

static render::command_buffer_t gComputeCommandBuffer;
static render::shader_t gVertexShader;
static render::shader_t gFragmentShader;
static render::shader_t gComputeShader;

static sample_utils::free_camera_t gCamera;
static maths::vec2 gMousePosition = vec2(0.0f,0.0f);
static bool gMouseButtonPressed = false;

static maths::uvec2 gImageSize = {1200u,800u};
static u32 gSampleCount = 0u;


static const char* gVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec2 aTexCoord;\n \
  out vec2 uv;\n \
  void main(void)\n \
  {\n \
    gl_Position = vec4(aPosition, 1.0);\n \
    uv = aTexCoord;\n \
  }\n"
};

static const char* gFragmentShaderSource = {
  "#version 440 core\n \
  in vec2 uv;\n \
  layout(binding = 0) uniform sampler2D uTexture; \n \
  layout(location = 0) out vec4 result; \n \
  void main(void)\n \
  {\n \
    vec4 texColor = texture(uTexture, uv);\n \
    vec3 color = texColor.rgb;\n \
    color = color / (color + vec3(1.0));\n\
    color = pow(color, vec3(1.0 / 2.2));\n\
    result = vec4(color, 1.0);\n\
  }\n"
};



struct Camera
{
  maths::mat4 tx;
  f32 verticalFov;
  f32 focalDistance;
  f32 aperture;
  f32 padding;
};

struct Material
{
  maths::vec3 albedo;
  float metalness;
  maths::vec3 F0;
  float roughness;
};

struct Sphere
{
  maths::vec3 origin;
  float radius;
  Material material;
};

struct Scene
{
  Sphere sphere[50];
  u32 sphereCount;
};

struct BufferData
{
  u32 sampleCount;
  u32 maxBounces;
  maths::uvec2 imageSize;
  Camera camera;
  Scene scene;
};

f32 Random()
{
  return (f32)(rand() / (RAND_MAX + 1.0));
}

void GenerateScene( u32 sphereCount, const maths::vec3& extents, Scene* scene )
{
  scene->sphereCount = sphereCount + 1;  //Sphere count + ground

  //Generate 5 materials
  Material materials[5];
  materials[0].albedo = vec3(0.8f,0.8f,0.8f);
  materials[0].roughness = 1.0f;
  materials[0].metalness = 0.0f;
  materials[0].F0 = vec3(0.02f,0.02f,0.02f);

  //Copper
  materials[1].albedo = vec3(0.0f,0.0f,0.0f);
  materials[1].roughness = 0.01f;
  materials[1].metalness = 1.0f;
  materials[1].F0 = vec3(0.95f,0.3f,0.3f);

  //Plastic
  materials[2].albedo = vec3(0.05f,0.85f,0.05f);
  materials[2].roughness = 0.1f;
  materials[2].metalness = 0.5f;
  materials[2].F0 = vec3(0.4f,0.4f,0.4f);

  //Gold
  materials[3].albedo = vec3(0.0f,0.0f,0.0f);
  materials[3].roughness = 0.05f;
  materials[3].metalness = 1.0f;
  materials[3].F0 = vec3(1.00f,0.71f,0.29f);

  //Iron
  materials[4].albedo = vec3(0.0f,0.0f,0.0f);
  materials[4].roughness = 0.1f;
  materials[4].metalness = 1.0f;
  materials[4].F0 = vec3(0.56f,0.57f,0.58f);

  //Ground
  scene->sphere[0].origin = maths::vec3(0.0f,-100000.0f,0.0f);
  scene->sphere[0].radius = 100000.0f - 1.0f;
  scene->sphere[0].material = materials[0];

  for( u32 i(1); i<sphereCount+1; ++i )
  {
    bool ok(false);
    f32 radius;
    vec3 center;
    do
    {
      ok = true;
      radius = Random() + 0.4f;
      center = vec3( (2.0f * Random() - 1.0f) * extents.x, radius - 1.0001f, (2.0f * Random() - 1.0f ) * extents.z);
      for( u32 j(0); j<i; ++j )
      {
        f32 distance = lenght( center - scene->sphere[j].origin );
        if( distance < radius + scene->sphere[j].radius )
        {
          ok = false;
          break;
        }
      }
    }while( !ok );

    scene->sphere[i].radius = radius;
    scene->sphere[i].origin = center;
    scene->sphere[i].material = materials[u32(Random() * 5 )];
  }
}

bool CreateResources()
{
  //Create the texture
  render::texture2DCreate( gContext, gImageSize.x, gImageSize.y, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, render::texture_sampler_t(), &gTexture );
  render::textureChangeLayoutNow(gContext, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &gTexture);

  //Create data to be passed to the gpu
  BufferData data;
  data.sampleCount = gSampleCount;
  data.maxBounces = 3;
  data.imageSize = gImageSize;
  data.camera.tx.setIdentity();
  data.camera.verticalFov = (f32)M_PI_2;
  data.camera.focalDistance = 5.0f;
  data.camera.aperture = 0.05f;
  GenerateScene( 45u, vec3(15.0f,0.0f,15.0f), &data.scene );

  //Create uniform buffer
  render::gpuBufferCreate( gContext, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                           render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                           (void*)&data, sizeof(data),
                           &gUbo );

  return true;
}

void CreateGeometry()
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: In Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { { { -1.0f, +1.0f, +0.0f }, { 0.0f, 0.0f } },
                                     { { +1.0f, +1.0f, +0.0f }, { 1.0f, 0.0f } },
                                     { { +1.0f, -1.0f, +0.0f }, { 1.0f, 1.0f } },
                                     { { -1.0f, -1.0f, +0.0f }, { 0.0f, 1.0f } }
                                    };

  static const uint32_t indices[] = {0,1,2,0,2,3};


  static render::vertex_attribute_t attributes[2];
  attributes[0].format_ = render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = render::vertex_attribute_t::format::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  mesh::create( gContext, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &gMesh );
}

void CreateGraphicsPipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t binding = { render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage::FRAGMENT };
  render::descriptorSetLayoutCreate( gContext, 1, &binding, &descriptorSetLayout );

  //Create pipeline layout  
  render::pipelineLayoutCreate( gContext, 1u, &descriptorSetLayout, &gPipelineLayout );

  //Create descriptor pool
  render::descriptorPoolCreate(gContext, 2u, 1u, 1u, 0u, 1u, &gDescriptorPool);

  //Create descriptor set
  render::descriptor_t descriptor = render::getDescriptor(gTexture);
  render::descriptorSetCreate( gContext, gDescriptorPool, descriptorSetLayout, &descriptor, &gDescriptorSet );

  //Load shaders
  bkk::render::shaderCreateFromGLSLSource(gContext, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &gVertexShader);
  bkk::render::shaderCreateFromGLSLSource(gContext, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &gFragmentShader);

  //Create graphics pipeline
  bkk::render::graphics_pipeline_t::description_t pipelineDesc;
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)gContext.swapChain_.imageWidth_, (float)gContext.swapChain_.imageHeight_, 0.0f, 1.0f };
  pipelineDesc.scissorRect_ = { {0,0}, {gContext.swapChain_.imageWidth_,gContext.swapChain_.imageHeight_} };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = false;
  pipelineDesc.depthWriteEnabled_ = false;
  pipelineDesc.vertexShader_ = gVertexShader;
  pipelineDesc.fragmentShader_ = gFragmentShader;
  render::graphicsPipelineCreate( gContext, gContext.swapChain_.renderPass_, 0u, gMesh.vertexFormat_, gPipelineLayout, pipelineDesc, &gPipeline );
}

void CreateComputePipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;

  render::descriptor_binding_t bindings[2] = { render::descriptor_binding_t{ render::descriptor_t::type::STORAGE_IMAGE,  0, render::descriptor_t::stage::COMPUTE },
                                               render::descriptor_binding_t{ render::descriptor_t::type::UNIFORM_BUFFER, 1, render::descriptor_t::stage::COMPUTE }};

  render::descriptorSetLayoutCreate(gContext, 2u, bindings, &descriptorSetLayout);

  //Create pipeline layout
  render::pipelineLayoutCreate(gContext, 1u, &descriptorSetLayout, &gComputePipelineLayout);

  //Create descriptor set
  render::descriptor_t descriptors[2] = { render::getDescriptor(gTexture), render::getDescriptor(gUbo) };
  render::descriptorSetCreate(gContext, gDescriptorPool, descriptorSetLayout, descriptors, &gComputeDescriptorSet);

  //Create pipeline
  bkk::render::shaderCreateFromGLSL(gContext, bkk::render::shader_t::COMPUTE_SHADER, "../path-tracing/path-tracing.comp", &gComputeShader);
  gComputePipeline.computeShader_ = gComputeShader;
  render::computePipelineCreate( gContext, gComputePipelineLayout, &gComputePipeline );
}

void CreatePipelines()
{
  CreateGraphicsPipeline();
  CreateComputePipeline();
}

void BuildCommandBuffers()
{
  for( unsigned i(0); i<3; ++i )
  {
    VkCommandBuffer cmdBuffer = render::beginPresentationCommandBuffer( gContext, i, nullptr );

    render::textureChangeLayout(gContext, cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &gTexture);
    
    bkk::render::graphicsPipelineBind(cmdBuffer, gPipeline);
    bkk::render::descriptorSetBindForGraphics(cmdBuffer, gPipelineLayout, 0, &gDescriptorSet, 1u);
    mesh::draw(cmdBuffer, gMesh );

    render::textureChangeLayout(gContext, cmdBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &gTexture);
    render::endPresentationCommandBuffer( gContext, i );
  }
}

void BuildComputeCommandBuffer()
{
  //Build compute command buffer
  render::commandBufferCreate( gContext, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 0u, nullptr, nullptr, 0u, nullptr, render::command_buffer_t::COMPUTE, &gComputeCommandBuffer );

  render::commandBufferBegin( nullptr, 0u, nullptr, gComputeCommandBuffer);
  
  bkk::render::computePipelineBind(gComputeCommandBuffer.handle_, gComputePipeline);
  bkk::render::descriptorSetBindForCompute(gComputeCommandBuffer.handle_, gComputePipelineLayout, 0, &gComputeDescriptorSet, 1u);

  vkCmdDispatch(gComputeCommandBuffer.handle_, gImageSize.x/16, gImageSize.y/16, 1);

  render::commandBufferEnd(gComputeCommandBuffer);
}

void Exit()
{
  //Wait for all pending operations to be finished
  render::contextFlush( gContext );

  //Destroy all resources
  render::commandBufferDestroy( gContext, &gComputeCommandBuffer );

  mesh::destroy( gContext, &gMesh );
  render::textureDestroy( gContext, &gTexture );
  render::gpuBufferDestroy( gContext, &gUbo );

  render::shaderDestroy(gContext, &gVertexShader);
  render::shaderDestroy(gContext, &gFragmentShader);
  render::shaderDestroy(gContext, &gComputeShader);

  render::graphicsPipelineDestroy( gContext, &gPipeline );
  render::descriptorSetDestroy( gContext, &gDescriptorSet );
  render::pipelineLayoutDestroy( gContext, &gPipelineLayout );

  render::computePipelineDestroy( gContext, &gComputePipeline );
  render::descriptorSetDestroy( gContext, &gComputeDescriptorSet );
  render::pipelineLayoutDestroy( gContext, &gComputePipelineLayout );

  render::descriptorPoolDestroy( gContext, &gDescriptorPool );

  render::contextDestroy( &gContext );

  //Close window
  window::destroy( &gWindow );
}

void Render()
{
  render::presentNextImage( &gContext );

  if (gSampleCount < 1000.0f)
  {
    //Submit compute command buffer
    render::gpuBufferUpdate(gContext, (void*)&gSampleCount, 0, sizeof(u32), &gUbo);
    ++gSampleCount;
    render::commandBufferSubmit(gContext, gComputeCommandBuffer);
    vkQueueWaitIdle(gContext.computeQueue_.handle_);
  }
}

void UpdateCameraTransform()
{
  render::gpuBufferUpdate( gContext, (void*)&gCamera.tx_, offsetof(BufferData,camera), sizeof(mat4), &gUbo );
  gSampleCount = 0;
}

void OnKeyEvent( window::key_e key, bool pressed )
{
  if( pressed )
  {
    switch( key )
    {
      case window::key_e::KEY_UP:
      case 'w':
      {
        gCamera.Move( 0.0f, -0.5f );
        UpdateCameraTransform();
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        gCamera.Move( 0.0f, 0.5f );
        UpdateCameraTransform();
        break;
      }
      case window::key_e::KEY_LEFT:
      case 'a':
      {
        gCamera.Move( -0.5f, 0.0f );
        UpdateCameraTransform();
        break;
      }
      case window::key_e::KEY_RIGHT:
      case 'd':
      {
        gCamera.Move( 0.5f, 0.0f );
        UpdateCameraTransform();
        break;
      }
      default:
        break;
    }
  }
}

void OnMouseButton( window::mouse_button_e button, uint32_t x, uint32_t y, bool pressed )
{
  gMouseButtonPressed = pressed;
  gMousePosition.x = (f32)x;
  gMousePosition.y = (f32)y;
}

void OnMouseMove( uint32_t x, uint32_t y )
{
  if( gMouseButtonPressed )
  {
    f32 angleY = (x - gMousePosition.x) * 0.01f;
    f32 angleX = (y - gMousePosition.y) * 0.01f;
    gCamera.Rotate( angleX, angleY );
    gMousePosition.x = (f32)x;
    gMousePosition.y = (f32)y;
    UpdateCameraTransform();
  }
}

int main()
{
  //Create a window
  window::create( "Path Tracer", gImageSize.x , gImageSize.y, &gWindow );

  //Initialize gContext
  render::contextCreate( "Path Tracer", "", gWindow, 3, &gContext );

  CreateGeometry();
  CreateResources();
  CreatePipelines();
  BuildCommandBuffers();
  BuildComputeCommandBuffer();

  bool quit = false;
  while( !quit )
  {
    window::event_t* event = nullptr;
    while( (event = window::getNextEvent( &gWindow )) )
    {
      switch( event->type_)
      {
        case window::EVENT_QUIT:
        {
          quit = true;
          break;
        }
        case window::EVENT_RESIZE:
        {
          window::event_resize_t* resizeEvent = (window::event_resize_t*)event;
          render::swapchainResize( &gContext, resizeEvent->width_, resizeEvent->height_ );
          BuildCommandBuffers();
          break;
        }
        case window::EVENT_KEY:
        {
          window::event_key_t* keyEvent = (window::event_key_t*)event;
          OnKeyEvent( keyEvent->keyCode_, keyEvent->pressed_ );
          break;
        }
        case window::EVENT_MOUSE_BUTTON:
        {
          window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;
          OnMouseButton( buttonEvent->button_, buttonEvent->x_, buttonEvent->y_, buttonEvent->pressed_ );
          break;
        }
        case window::EVENT_MOUSE_MOVE:
        {
          window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
          OnMouseMove( moveEvent->x_, moveEvent->y_);
          break;
        }
        default:
          break;
      }
    }

    Render();
  }


  Exit();

  return 0;
}
