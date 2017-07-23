
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "maths.h"
#include "utility.h"

using namespace bkk;
using namespace maths;

static window::window_t gWindow;
static render::context_t gContext;
static render::texture_t gTexture;
static render::gpu_buffer_t gUbo;
static mesh::mesh_t gMesh;
static render::descriptor_pool_t gDescriptorPool;
static render::pipeline_layout_t gPipelineLayout;
static render::descriptor_set_t gDescriptorSet;
static render::graphics_pipeline_t gPipeline;
static VkShaderModule gVertexShader;
static VkShaderModule gFragmentShader;

static sample_utils::orbiting_camera_t gCamera;
static maths::vec2 gMousePosition = vec2(0.0f,0.0f);
static bool gMouseButtonPressed = false;
static maths::mat4 gProjection;
static maths::mat4 gModelTransform;

bool CreateResources()
{
  image::image2D_t image = {};
  if( !image::Load( "./resources/r2d2_diffuse.png", &image ))
  {
    printf("Error loading texture\n" );
    return false;
  }

  //Create the texture
  render::texture_sampler_t sampler = {};
  sampler.minification_ = render::filter_mode_e::LINEAR;
  sampler.magnification_ = render::filter_mode_e::LINEAR;
  sampler.wrapU_ = render::wrap_mode_e::CLAMP_TO_EDGE;
  sampler.wrapV_ = render::wrap_mode_e::CLAMP_TO_EDGE;

  render::TextureCreate( &gContext, &image, 1, sampler, &gTexture );
  image::Unload( &image );

  //Create uniform buffer
  gCamera.offset_ = 1.5f;
  gCamera.Update();


  gModelTransform.SetIdentity();
  gModelTransform.SetTranslation( vec3( 0.0f, -1.0f, 0.0f ) );
  gProjection = ComputePerspectiveProjectionMatrix( 1.5f,(f32)gWindow.width_ / (f32)gWindow.height_,0.1f,100.0f );
  mat4 modelViewProjection = gModelTransform * gCamera.view_ * gProjection;
  render::GpuBufferCreate( &gContext, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                            render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                            (void*)&modelViewProjection, sizeof(mat4),
                            &gUbo );
  return true;
}

void UpdateUniformBuffer()
{
  mat4 modelViewProjection = gModelTransform * gCamera.view_ * gProjection;
  render::GpuBufferUpdate( &gContext, (void*)&modelViewProjection, 0, sizeof(mat4), &gUbo );
}

void CreateGeometry()
{
  mesh::CreateFromFile( &gContext, "./resources/r2d2.dae", &gMesh );
}

void CreatePipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  descriptorSetLayout.bindings_.push_back( { render::descriptor_type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_stage_e::FRAGMENT } );
  descriptorSetLayout.bindings_.push_back( { render::descriptor_type_e::UNIFORM_BUFFER, 1, render::descriptor_stage_e::VERTEX } );
  render::DescriptorSetLayoutCreate( &gContext, &descriptorSetLayout );

  //Create pipeline layout
  gPipelineLayout.descriptorSetLayout_.push_back( descriptorSetLayout );
  render::PipelineLayoutCreate( &gContext, &gPipelineLayout );

  //Create descriptor pool
  gDescriptorPool = {};
  gDescriptorPool.combinedImageSamplers_ = 1;
  gDescriptorPool.uniformBuffers_ = 1;
  gDescriptorPool.descriptorSets_ = 1;
  render::DescriptorPoolCreate( &gContext, &gDescriptorPool );

  //Create descriptor set
  gDescriptorSet.descriptors_.resize(2);
  gDescriptorSet.descriptors_[0].imageDescriptor_ = gTexture.descriptor_;
  gDescriptorSet.descriptors_[1].bufferDescriptor_ = gUbo.descriptor_;
  render::DescriptorSetCreate( &gContext, &gDescriptorPool, &descriptorSetLayout, &gDescriptorSet );

  //Load shaders
  gVertexShader   = LoadShader( &gContext , "shaders/model.vert.spv" );
  gFragmentShader = LoadShader( &gContext, "shaders/model.frag.spv" );

  //Create pipeline
  gPipeline.viewPort_ = { 0.0f, 0.0f, (float)gContext.swapChain_.imageWidth_, (float)gContext.swapChain_.imageHeight_, 0.0f, 1.0f };
  gPipeline.scissorRect_ = { {0,0}, {gContext.swapChain_.imageWidth_,gContext.swapChain_.imageHeight_} };
  gPipeline.blendState_.resize(1);
  gPipeline.blendState_[0].colorWriteMask = 0xF;
  gPipeline.blendState_[0].blendEnable = VK_FALSE;
  gPipeline.cullMode_ = VK_CULL_MODE_BACK_BIT;
  gPipeline.depthTestEnabled_ = true;
  gPipeline.depthWriteEnabled_ = true;
  gPipeline.depthTestFunction_ = VK_COMPARE_OP_LESS_OR_EQUAL;
  gPipeline.vertexShader_ = gVertexShader;
  gPipeline.fragmentShader_ = gFragmentShader;
  render::GraphicsPipelineCreate( &gContext, gContext.swapChain_.renderPass_, &gMesh.vertexFormat_, &gPipelineLayout, &gPipeline );
}

void BuildCommandBuffers()
{
  for( unsigned i(0); i<3; ++i )
  {
    VkCommandBuffer cmdBuffer = render::BeginPresentationCommandBuffer( &gContext, i, nullptr );
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,gPipeline.handle_);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout.handle_, 0, 1, &gDescriptorSet.handle_, 0, nullptr);
    mesh::Draw(cmdBuffer, &gMesh );
    render::EndPresentationCommandBuffer( &gContext, i );
  }
}

void Exit()
{
  //Wait for all pending operations to be finished
  render::Flush( &gContext );

  //Destroy all resources
  mesh::Destroy( &gContext, &gMesh );
  render::TextureDestroy( &gContext, &gTexture );
  render::GpuBufferDestroy( &gContext, &gUbo );
  vkDestroyShaderModule(gContext.device_, gVertexShader, nullptr);
  vkDestroyShaderModule(gContext.device_, gFragmentShader, nullptr);

  render::GraphicsPipelineDestroy( &gContext, &gPipeline );
  render::DescriptorSetDestroy( &gContext, &gDescriptorSet );
  render::DescriptorPoolDestroy( &gContext, &gDescriptorPool );
  render::PipelineLayoutDestroy( &gContext, &gPipelineLayout );

  render::ContextDestroy( &gContext );
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
        gCamera.Move( -.5f );
        UpdateUniformBuffer();
        break;
      }
      case window::key_e::KEY_DOWN:
      case 's':
      {
        gCamera.Move( .5f );
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
  window::Initialize( "Model", 400u, 400u, &gWindow );

  //Initialize gContext
  render::ContextCreate( "Model", "", &gWindow, 3, &gContext );

  CreateResources();
  CreateGeometry();
  CreatePipeline();
  BuildCommandBuffers();

  bool quit = false;
  while( !quit )
  {
    window::event_t* event = nullptr;
    while( (event = window::GetNextEvent( &gWindow )) )
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
          render::ContextResize( &gContext, resizeEvent->width_, resizeEvent->height_ );
          BuildCommandBuffers( );
          gProjection = ComputePerspectiveProjectionMatrix( 1.5f,(f32)resizeEvent->width_ / (f32)resizeEvent->height_ ,0.1f,100.0f );
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

    //Render next image
    render::PresentNextImage( &gContext );
  }

  Exit();
  return 0;
}
