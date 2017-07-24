
#include "render.h"
#include "window.h"
#include "image.h"
#include "mesh.h"
#include "../utility.h"
#include <cstddef>
#include <cstdlib>
#include <time.h>
#include <math.h>

#include "distance-field.h"

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
static render::gpu_buffer_t gSsbo;

static VkCommandBuffer gComputeCommandBuffer;
static render::shader_t gVertexShader;
static render::shader_t gFragmentShader;
static render::shader_t gComputeShader;

static sample_utils::free_camera_t gCamera;
static maths::vec2 gMousePosition = vec2(0.0f, 0.0f);
static bool gMouseButtonPressed = false;

static maths::uvec2 gImageSize = { 1200u,800u };
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
	layout(location = 0) out vec4 color; \n \
	void main(void)\n \
	{\n \
		vec4 texColor = texture(uTexture, uv);\n \
		color = texColor;\n \
	}\n"
};


static mesh_t* CreateCube(u32 width, u32 height, u32 depth)
{
  float hw = width / 2.0f;
  float hh = height / 2.0f;
  float hd = depth / 2.0f;

  dfvec3 vertex[8] = { { -hw,-hh,hd },{ hw,-hh,hd },{ -hw,hh,hd },{ hw,hh,hd },
  { -hw,-hh,-hd },{ hw,-hh,-hd },{ -hw,hh,-hd },{ hw,hh,-hd } };

  u32 index[36] = { 0,1,2, 1,3,2,  1,5,3, 5,7,3,
    4,0,6, 0,2,6,  5,4,7, 4,6,7,
    2,3,6, 3,7,6,  4,5,0, 5,1,0 };

  AABB_t aabb = { { -hw,-hh,hd },{ hw,hh,-hd } };

  mesh_t* mesh;
  meshCreate(&vertex[0], 8u, &index[0], 36, aabb, &mesh);

  return mesh;
}

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

struct DistanceField
{
  mat4 tx;
  u32 width;
  u32 height;
  u32 depth;
  u32 padding;
  vec4 aabbMin;
  vec4 aabbMax;
};

struct BufferData
{
  u32 sampleCount;
  u32 maxBounces;
  maths::uvec2 imageSize;
  Camera camera;
  DistanceField distanceField;
};

bool CreateResources()
{
  //Create the texture
  render::texture_sampler_t sampler = {};
  sampler.minification_ = render::filter_mode_e::LINEAR;
  sampler.magnification_ = render::filter_mode_e::LINEAR;
  sampler.wrapU_ = render::wrap_mode_e::CLAMP_TO_EDGE;
  sampler.wrapV_ = render::wrap_mode_e::CLAMP_TO_EDGE;

  render::texture2DCreate(gContext, gImageSize.x, gImageSize.y, 4, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, sampler, &gTexture);
  render::textureChangeLayout(gContext, gContext.initializationCmdBuffer_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, &gTexture);
  render::initResources(gContext);

  //Create data to be passed to the gpu
  BufferData data;
  data.sampleCount = gSampleCount;
  data.maxBounces = 3;
  data.imageSize = gImageSize;
  data.camera.tx.setIdentity();
  //data.camera.tx.SetTranslation( vec3(0.0,0.0,0.0) );
  data.camera.verticalFov = (f32)M_PI_2;
  data.camera.focalDistance = 5.0f;
  data.camera.aperture = 0.05f;

  mesh_t* cube = CreateCube(1.0, 1.0, 1.0);
  distance_field_t* df;
  //distanceFieldCreate(50, 50, 50, 1.0f, &df);
  distanceFieldCreate(50, 50, 50, cube, &df);
 
  data.distanceField.tx.setIdentity();
  //data.distanceField.tx.SetTranslation(vec3(20.0f,20.0f, 20.0f)); //Sphere
  //data.distanceField.tx.SetTranslation(vec3(95.0f, 95.0f, 30.0f));  //Cube
  data.distanceField.width = df->width;
  data.distanceField.height = df->height;
  data.distanceField.depth = df->depth;
  data.distanceField.aabbMin = maths::vec4(df->aabbMin.x, df->aabbMin.y, df->aabbMin.z, 0.0f);
  data.distanceField.aabbMax = maths::vec4(df->aabbMax.x, df->aabbMax.y, df->aabbMax.z, 0.0f);
  
  //Create uniform buffer
  render::gpuBufferCreate(gContext, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    (void*)&data, sizeof(data),
    &gUbo);

  render::gpuBufferCreate(gContext, render::gpu_buffer_usage_e::STORAGE_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    (void*)df->data, sizeof(float)*df->width *df->height * df->depth,
    &gSsbo);

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
  static const Vertex vertices[] = { { { -1.0f, +1.0f, +0.0f },{ 0.0f, 0.0f } },
  { { +1.0f, +1.0f, +0.0f },{ 1.0f, 0.0f } },
  { { +1.0f, -1.0f, +0.0f },{ 1.0f, 1.0f } },
  { { -1.0f, -1.0f, +0.0f },{ 0.0f, 1.0f } }
  };

  static const uint32_t indices[] = { 0,1,2,0,2,3 };


  static render::vertex_attribute_t attributes[2];
  attributes[0].format_ = render::attribute_format_e::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = render::attribute_format_e::VEC2;;
  attributes[1].offset_ = 3 * sizeof(float);
  attributes[1].stride_ = sizeof(Vertex);

  mesh::create(gContext, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &gMesh);
}

void CreateGraphicsPipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  descriptorSetLayout.bindings_.push_back({ render::descriptor_type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_stage_e::FRAGMENT });
  render::descriptorSetLayoutCreate(gContext, &descriptorSetLayout);

  //Create pipeline layout
  gPipelineLayout.descriptorSetLayout_.push_back(descriptorSetLayout);
  render::pipelineLayoutCreate(gContext, &gPipelineLayout);

  //Create descriptor pool
  gDescriptorPool = {};
  gDescriptorPool.combinedImageSamplers_ = 1;
  gDescriptorPool.storageBuffers_ = 1;
  gDescriptorPool.uniformBuffers_ = 1;
  gDescriptorPool.storageImages_ = 1;
  gDescriptorPool.descriptorSets_ = 2;
  render::descriptorPoolCreate(gContext, &gDescriptorPool);

  //Create descriptor set
  gDescriptorSet.descriptors_.resize(1);
  gDescriptorSet.descriptors_[0].imageDescriptor_ = gTexture.descriptor_;
  render::descriptorSetCreate(gContext, gDescriptorPool, descriptorSetLayout, &gDescriptorSet);

  //Load shaders
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::VERTEX_SHADER, gVertexShaderSource, &gVertexShader);
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &gFragmentShader);

  //Create graphics pipeline
  gPipeline.viewPort_ = { 0.0f, 0.0f, (float)gContext.swapChain_.imageWidth_, (float)gContext.swapChain_.imageHeight_, 0.0f, 1.0f };
  gPipeline.scissorRect_ = { { 0,0 },{ gContext.swapChain_.imageWidth_,gContext.swapChain_.imageHeight_ } };
  gPipeline.blendState_.resize(1);
  gPipeline.blendState_[0].colorWriteMask = 0xF;
  gPipeline.blendState_[0].blendEnable = VK_FALSE;
  gPipeline.cullMode_ = VK_CULL_MODE_BACK_BIT;
  gPipeline.depthTestEnabled_ = false;
  gPipeline.depthWriteEnabled_ = false;
  gPipeline.vertexShader_ = gVertexShader.handle_;
  gPipeline.fragmentShader_ = gFragmentShader.handle_;
  render::graphicsPipelineCreate(gContext, gContext.swapChain_.renderPass_, gMesh.vertexFormat_, gPipelineLayout, &gPipeline);
}

void CreateComputePipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  descriptorSetLayout.bindings_.push_back({ render::descriptor_type_e::STORAGE_IMAGE,  0, render::descriptor_stage_e::COMPUTE });
  descriptorSetLayout.bindings_.push_back({ render::descriptor_type_e::UNIFORM_BUFFER, 1, render::descriptor_stage_e::COMPUTE });
  descriptorSetLayout.bindings_.push_back({ render::descriptor_type_e::STORAGE_BUFFER, 2, render::descriptor_stage_e::COMPUTE });
  render::descriptorSetLayoutCreate(gContext, &descriptorSetLayout);

  //Create pipeline layout
  gComputePipelineLayout.descriptorSetLayout_.push_back(descriptorSetLayout);
  render::pipelineLayoutCreate(gContext, &gComputePipelineLayout);

  //Create descriptor set
  gComputeDescriptorSet.descriptors_.resize(3);
  gComputeDescriptorSet.descriptors_[0].imageDescriptor_ = gTexture.descriptor_;
  gComputeDescriptorSet.descriptors_[1].bufferDescriptor_ = gUbo.descriptor_;
  gComputeDescriptorSet.descriptors_[2].bufferDescriptor_ = gSsbo.descriptor_;
  render::descriptorSetCreate(gContext, gDescriptorPool, descriptorSetLayout, &gComputeDescriptorSet);

  //Create pipeline
  bkk::render::shaderCreateFromGLSL(gContext, bkk::render::shader_t::COMPUTE_SHADER, "../distance-field/distance-field.comp", &gComputeShader);
  gComputePipeline.computeShader_ = gComputeShader.handle_;
  render::computePipelineCreate(gContext, gComputePipelineLayout, &gComputePipeline);
}

void CreatePipelines()
{
  CreateGraphicsPipeline();
  CreateComputePipeline();
}

void BuildCommandBuffers()
{
  for (unsigned i(0); i<3; ++i)
  {
    VkCommandBuffer cmdBuffer = render::beginPresentationCommandBuffer(gContext, i, nullptr);

    //Image memory barrier to make sure compute shader has finished
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.image = gTexture.image_;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    vkCmdPipelineBarrier(
      cmdBuffer,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipeline.handle_);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gPipelineLayout.handle_, 0, 1, &gDescriptorSet.handle_, 0, nullptr);
    mesh::draw(cmdBuffer, gMesh);

    barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = gTexture.image_;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(
      cmdBuffer,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier);

    render::endPresentationCommandBuffer(gContext, i);
  }
}

void BuildComputeCommandBuffer()
{
  //Build compute command buffer
  render::allocateCommandBuffers(gContext, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1, &gComputeCommandBuffer);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  vkBeginCommandBuffer(gComputeCommandBuffer, &beginInfo);


  vkCmdBindPipeline(gComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gComputePipeline.handle_);
  vkCmdBindDescriptorSets(gComputeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, gComputePipelineLayout.handle_, 0, 1, &gComputeDescriptorSet.handle_, 0, 0);

  vkCmdDispatch(gComputeCommandBuffer, gImageSize.x / 16, gImageSize.y / 16, 1);

  vkEndCommandBuffer(gComputeCommandBuffer);
}

void Exit()
{
  //Wait for all pending operations to be finished
  render::contextFlush(gContext);

  //Destroy all resources
  render::freeCommandBuffers(gContext, 1, &gComputeCommandBuffer);

  mesh::destroy(gContext, &gMesh);
  render::textureDestroy(gContext, &gTexture);
  render::gpuBufferDestroy(gContext, &gUbo);

  render::shaderDestroy(gContext, &gVertexShader);
  render::shaderDestroy(gContext, &gFragmentShader);
  render::shaderDestroy(gContext, &gComputeShader);

  render::graphicsPipelineDestroy(gContext, &gPipeline);
  render::descriptorSetDestroy(gContext, &gDescriptorSet);
  render::pipelineLayoutDestroy(gContext, &gPipelineLayout);

  render::computePipelineDestroy(gContext, &gComputePipeline);
  render::descriptorSetDestroy(gContext, &gComputeDescriptorSet);
  render::pipelineLayoutDestroy(gContext, &gComputePipelineLayout);

  render::descriptorPoolDestroy(gContext, &gDescriptorPool);

  render::contextDestroy(&gContext);

  //Close window
  window::destroy(&gWindow);
}

void Render()
{
  //render::gpuBufferUpdate(gContext, (void*)&gSampleCount, 0, sizeof(u32), &gUbo);
  ++gSampleCount;

  render::presentNextImage(&gContext);

  //Submit compute command buffer
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pNext = NULL;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &gComputeCommandBuffer;
  vkQueueSubmit(gContext.computeQueue_.handle_, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(gContext.computeQueue_.handle_);
}

void UpdateCameraTransform()
{
  render::gpuBufferUpdate(gContext, (void*)&gCamera.tx_, offsetof(BufferData, camera), sizeof(mat4), &gUbo);
  gSampleCount = 0;
}

void OnKeyEvent(window::key_e key, bool pressed)
{
  if (pressed)
  {
    switch (key)
    {
    case window::key_e::KEY_UP:
    case 'w':
    {
      gCamera.Move(0.0f, -2.5f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_DOWN:
    case 's':
    {
      gCamera.Move(0.0f, 2.5f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_LEFT:
    case 'a':
    {
      gCamera.Move(2.5f, 0.0f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_RIGHT:
    case 'd':
    {
      gCamera.Move(-2.5f, 0.0f);
      UpdateCameraTransform();
      break;
    }
    default:
      break;
    }
  }
}

void OnMouseButton(window::mouse_button_e button, uint32_t x, uint32_t y, bool pressed)
{
  gMouseButtonPressed = pressed;
  gMousePosition.x = (f32)x;
  gMousePosition.y = (f32)y;
}

void OnMouseMove(uint32_t x, uint32_t y)
{
  if (gMouseButtonPressed)
  {
    f32 angleY = (x - gMousePosition.x) * 0.01f;
    f32 angleX = (y - gMousePosition.y) * 0.01f;
    gCamera.Rotate(angleX, angleY);
    gMousePosition.x = (f32)x;
    gMousePosition.y = (f32)y;
    UpdateCameraTransform();
  }
}

int main()
{
  //Create a window
  window::create("Path Tracer", gImageSize.x, gImageSize.y, &gWindow);

  //Initialize gContext
  render::contextCreate("Path Tracer", "", &gWindow, 3, &gContext);

  CreateGeometry();
  CreateResources();
  CreatePipelines();
  BuildCommandBuffers();
  BuildComputeCommandBuffer();

  bool quit = false;
  while (!quit)
  {
    window::event_t* event = nullptr;
    while ((event = window::getNextEvent(&gWindow)))
    {
      switch (event->type_)
      {
      case window::EVENT_QUIT:
      {
        quit = true;
        break;
      }
      case window::EVENT_RESIZE:
      {
        window::event_resize_t* resizeEvent = (window::event_resize_t*)event;
        render::swapchainResize(&gContext, resizeEvent->width_, resizeEvent->height_);
        BuildCommandBuffers();
        break;
      }
      case window::EVENT_KEY:
      {
        window::event_key_t* keyEvent = (window::event_key_t*)event;
        OnKeyEvent(keyEvent->keyCode_, keyEvent->pressed_);
        break;
      }
      case window::EVENT_MOUSE_BUTTON:
      {
        window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;
        OnMouseButton(buttonEvent->button_, buttonEvent->x_, buttonEvent->y_, buttonEvent->pressed_);
        break;
      }
      case window::EVENT_MOUSE_MOVE:
      {
        window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
        OnMouseMove(moveEvent->x_, moveEvent->y_);
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
