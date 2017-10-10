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
#include "maths.h"
#include "../utility.h"
#include "timer.h"
#include <array>

using namespace bkk;
using namespace maths;

static render::context_t gContext;
static window::window_t gWindow;
static render::gpu_buffer_t gUbo;
static mesh::mesh_t gMesh;
static mesh::skeletal_animator_t gAnimator;
static maths::mat4 gModelTransform;
static maths::mat4 gProjection;
static render::pipeline_layout_t gPipelineLayout;
static render::descriptor_pool_t gDescriptorPool;
static render::descriptor_set_t gDescriptorSet;
static render::graphics_pipeline_t gPipeline;
static render::shader_t gVertexShader;
static render::shader_t gFragmentShader;

static sample_utils::orbiting_camera_t gCamera;
static maths::vec2 gMousePosition = vec2(0.0f,0.0f);
static bool gMouseButtonPressed = false;

static const char* gVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec3 aNormal;\n \
  layout(location = 2) in vec2 aTexCoord;\n \
  layout(location = 3) in vec4 aBonesWeight;\n \
  layout(location = 4) in vec4 aBonesId;\n \
  layout(binding = 1) uniform UNIFORMS\n \
  {\n \
    mat4 modelView;\n \
    mat4 modelViewProjection;\n \
  }uniforms;\n \
  layout(binding = 2) uniform BONESTX\n \
  {\n \
    mat4 bones[64];\n \
  }bonesTx;\n \
  out vec3 normalViewSpace;\n \
  out vec3 lightViewSpace;\n \
  void main(void)\n \
  {\n \
    mat4 transform = bonesTx.bones[int(aBonesId[0])] * aBonesWeight[0] +  \n \
    bonesTx.bones[int(aBonesId[1])] * aBonesWeight[1] +					  \n \
    bonesTx.bones[int(aBonesId[2])] * aBonesWeight[2] +					  \n \
    bonesTx.bones[int(aBonesId[3])] * aBonesWeight[3];                    \n \
    gl_Position = uniforms.modelViewProjection * transform * vec4(aPosition,1.0); \n \
    mat4 normalTransform = mat4(inverse(transpose(uniforms.modelView * transform)));\n \
    normalViewSpace = normalize((normalTransform * vec4(aNormal,0.0)).xyz);\n \
    vec3 lightPositionModelSpace = vec3(-0.5,0.5,1.0);\n \
    lightViewSpace = normalize((uniforms.modelView * vec4(normalize(lightPositionModelSpace),0.0)).xyz);\n \
  }\n"
};

static const char* gFragmentShaderSource = {
  "#version 440 core\n \
  in vec3 normalViewSpace;\n \
  in vec3 lightViewSpace;\n \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    float diffuse = max(dot(normalize(lightViewSpace), normalize(normalViewSpace)), 0.0);\n \
    color = vec4(vec3(diffuse), 1.0);\n \
  }\n"
};

bool CreateUniformBuffer()
{
  gCamera.offset_ = 35.0f;
  gCamera.angle_ = vec2(-0.8f,0.0f);
  gCamera.Update();

  gProjection = computePerspectiveProjectionMatrix( 1.5f,(f32)gWindow.width_ / (f32)gWindow.height_,1.0f,1000.0f );
  gModelTransform = computeTransform( VEC3_ZERO, vec3( 0.01f,0.01f,0.01f), quaternionFromAxisAngle( vec3(1.0f,0.0f,0.0f), degreeToRadian(90.0f) ) );

  mat4 matrices[2];
  matrices[0] = gModelTransform * gCamera.view_;
  matrices[1] = matrices[0] * gProjection;

  //Create uniform buffer
  render::gpuBufferCreate( gContext, render::gpu_buffer_t::usage::UNIFORM_BUFFER,
                           render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                           (void*)&matrices, 2*sizeof(mat4),
                           &gUbo );
  return true;
}

void UpdateUniformBuffer()
{
  mat4 matrices[2];
  matrices[0] = gModelTransform * gCamera.view_;
  matrices[1] = matrices[0] * gProjection;

  render::gpuBufferUpdate( gContext, (void*)&matrices, 0, 2*sizeof(mat4), &gUbo );
}

void CreateGeometry()
{
  mesh::createFromFile( gContext, "../resources/goblin.dae", &gMesh );
  mesh::animatorCreate( gContext, gMesh, 0u, 5.0f, &gAnimator );
}

void CreatePipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  std::array< render::descriptor_binding_t, 2> bindings{ 
    render::descriptor_binding_t{ render::descriptor_t::type::UNIFORM_BUFFER, 1, render::descriptor_t::stage::VERTEX },
    render::descriptor_binding_t{ render::descriptor_t::type::UNIFORM_BUFFER, 2, render::descriptor_t::stage::VERTEX }
  };
 
  render::descriptorSetLayoutCreate( gContext, (uint32_t)bindings.size(), &bindings[0], &descriptorSetLayout );

  //Create pipeline layout
  render::pipelineLayoutCreate( gContext, 1u, &descriptorSetLayout, &gPipelineLayout );

  //Create descriptor pool
  render::descriptorPoolCreate( gContext, 1u, 0u, 2u, 0u, 0u, &gDescriptorPool );

  //Create descriptor set
  std::array<render::descriptor_t, 2> descriptors = { render::getDescriptor(gUbo), render::getDescriptor(gAnimator.buffer_)};  
  render::descriptorSetCreate( gContext, gDescriptorPool, descriptorSetLayout, &descriptors[0], &gDescriptorSet );

  //Load shaders
  bkk::render::shaderCreateFromGLSLSource(gContext, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &gVertexShader);
  bkk::render::shaderCreateFromGLSLSource(gContext, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &gFragmentShader);

  //Create pipeline
  bkk::render::graphics_pipeline_t::description_t pipelineDesc;
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)gContext.swapChain_.imageWidth_, (float)gContext.swapChain_.imageHeight_, 0.0f, 1.0f};
  pipelineDesc.scissorRect_ = { {0,0}, {gContext.swapChain_.imageWidth_,gContext.swapChain_.imageHeight_} };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = true;
  pipelineDesc.depthWriteEnabled_ = true;
  pipelineDesc.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
  pipelineDesc.vertexShader_ = gVertexShader;
  pipelineDesc.fragmentShader_ = gFragmentShader;
  render::graphicsPipelineCreate( gContext, gContext.swapChain_.renderPass_, gMesh.vertexFormat_, gPipelineLayout, pipelineDesc, &gPipeline );
}

void BuildCommandBuffers()
{
  for( unsigned i(0); i<3; ++i )
  {
    VkCommandBuffer cmdBuffer = render::beginPresentationCommandBuffer( gContext, i, nullptr );
    bkk::render::graphicsPipelineBind(cmdBuffer, gPipeline);
    bkk::render::descriptorSetBindForGraphics(cmdBuffer, gPipelineLayout, 0, &gDescriptorSet, 1u);
    mesh::draw(cmdBuffer, gMesh );
    render::endPresentationCommandBuffer( gContext, i );
  }
}

void Exit()
{
  //Wait for all pending operations to be finished
  render::contextFlush( gContext );

  //Destroy all resources
  mesh::destroy( gContext, &gMesh );
  mesh::animatorDestroy( gContext, &gAnimator );
  render::gpuBufferDestroy( gContext, &gUbo );

  render::shaderDestroy(gContext, &gVertexShader);
  render::shaderDestroy(gContext, &gFragmentShader);

  render::graphicsPipelineDestroy( gContext, &gPipeline );
  render::descriptorSetDestroy( gContext, &gDescriptorSet );
  render::descriptorPoolDestroy( gContext, &gDescriptorPool );
  render::pipelineLayoutDestroy( gContext, &gPipelineLayout );

  render::contextDestroy( &gContext );

  //Close window
  window::destroy( &gWindow );
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
        gCamera.Move( -1.0f );
        UpdateUniformBuffer();
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        gCamera.Move( 1.0f );
        UpdateUniformBuffer();
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
    gMousePosition.x = (f32)x;
    gMousePosition.y = (f32)y;
    gCamera.Rotate( angleY,angleX );
    UpdateUniformBuffer();
  }
}

int main()
{
  //Create a window
  gWindow = {};
  window::create( "Skinning", 400u, 400u, &gWindow );

  //Initialize gContext
  gContext = {};
  render::contextCreate( "Skinning", "", gWindow, 3, &gContext );

  CreateUniformBuffer();
  CreateGeometry();
  CreatePipeline();
  BuildCommandBuffers();

  auto timePrev = bkk::time::getCurrent();
  auto currentTime = timePrev;
  f32 timeInSecond = 0;

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
          gProjection = computePerspectiveProjectionMatrix( 1.5f,(f32)resizeEvent->width_ / (f32)resizeEvent->height_ ,1.0f,1000.0f );
          UpdateUniformBuffer();
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

    //Update animator
    currentTime = bkk::time::getCurrent();
    timeInSecond += bkk::time::getDifference( timePrev, currentTime ) * 0.001f;
    mesh::animatorUpdate( gContext, timeInSecond, &gAnimator );
    render::presentNextImage( &gContext );
    timePrev = currentTime;
  }

  Exit();
  return 0;
}
