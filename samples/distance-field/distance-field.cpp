
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


struct Camera
{
  maths::mat4 tx;
  f32 verticalFov;
  f32 focalDistance;
  f32 aperture;
};

struct UniformBufferData
{
  u32 sampleCount;
  u32 maxBounces;
  maths::uvec2 imageSize;
  Camera camera;
};

static render::context_t gContext;
static window::window_t gWindow;
static render::texture_t gTexture;
static mesh::mesh_t gFSQuad;

static render::descriptor_pool_t gDescriptorPool;

static render::pipeline_layout_t gPipelineLayout;
static render::descriptor_set_t gDescriptorSet;
static render::graphics_pipeline_t gPipeline;


static render::pipeline_layout_t gComputePipelineLayout;
static render::descriptor_set_t gComputeDescriptorSet;
static render::compute_pipeline_t gComputePipeline;
static render::gpu_buffer_t gUbo;
static render::gpu_buffer_t gDistanceField;

static VkCommandBuffer gComputeCommandBuffer;
static render::shader_t gVertexShader;
static render::shader_t gFragmentShader;
static render::shader_t gComputeShader;

static sample_utils::free_camera_t gCamera( vec3(0.0f,0.0f,5.0f), vec2(0.0f,0.0f), 1.0f);
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


static bkk::mesh::mesh_t CreateCube(const render::context_t& context, u32 width, u32 height, u32 depth)
{
  float hw = width / 2.0f;
  float hh = height / 2.0f;
  float hd = depth / 2.0f;

  vec3 vertices[8] = { { -hw,-hh,hd },{ hw,-hh,hd },{ -hw,hh,hd },{ hw,hh,hd },{ -hw,-hh,-hd },{ hw,-hh,-hd },{ -hw,hh,-hd },{ hw,hh,-hd } };
  u32 indices[36] = { 0,1,2, 1,3,2,  1,5,3, 5,7,3,
                      4,0,6, 0,2,6,  5,4,7, 4,6,7,
                      2,3,6, 3,7,6,  4,5,0, 5,1,0 };


  static bkk::render::vertex_attribute_t attributes[1];
  attributes[0].format_ = bkk::render::attribute_format_e::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(vec3);

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 1, &mesh);

  mesh.aabb_.min_ = vec3(-hw, -hh, -hd);
  mesh.aabb_.max_ = vec3(hw, hh, hd);
  return mesh;
}

static maths::vec3 closestPointOnTriangle(const maths::vec3& p, const maths::vec3& a, const maths::vec3& b, const maths::vec3& c)
{
  maths::vec3 ab = b - a;
  maths::vec3 ac = c - a;
  maths::vec3 ap = p - a;

  float d1 = maths::dot(ab, ap);
  float d2 = maths::dot(ac, ap);

  if (d1 <= 0.0f && d2 < 0.0f)
    return a;

  maths::vec3 bp = p - b;
  float d3 = maths::dot(ab, bp);
  float d4 = maths::dot(ac, bp);
  if (d3 >= 0.0f && d4 <= d3)
    return b;

  float vc = d1*d4 - d3*d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
  {
    float v = d1 / (d1 - d3);
    return a + ab * v;
  }

  maths::vec3 cp = p - c;
  float d5 = maths::dot(ab, cp);
  float d6 = maths::dot(ac, cp);
  if (d6 >= 0.0f && d5 <= d6)
    return c;

  float vb = d5*d2 - d1*d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
  {
    float w = d2 / (d2 - d6);
    return a + ac * w;
  }

  float va = d3*d6 - d5*d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
  {
    float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return b + (c - b) * w;
  }

  float denom = 1.0f / (va + vb + vc);
  float v = vb * denom;
  float w = vc * denom;

  return a + ab*v + ac*w;
}

static float signedDistancePointTriangle(const maths::vec3& point, const maths::vec3& a, const maths::vec3& b, const maths::vec3& c)
{
  //Find vector from point to closest point in triangle
  maths::vec3 v = closestPointOnTriangle(point, a, b, c) - point;

  //Compute sign of the distance (positive if in front, negative if behind)
  maths::vec3 normal = cross(b-a, c-a);
  float sign = dot(normal, v) < 0.0f ? 1.0f : -1.0f;

  return sign * lenght(v);
}

static float signedDistancePointMesh(const maths::vec3& point, uint32_t* index, uint32_t indexCount, vec3* vertex, uint32_t vertexCount )
{
  float minDistance = 10000.0f;
  for (u32 i = 0; i<indexCount; i += 3)
  {
    float d = signedDistancePointTriangle(point, vertex[index[i]], vertex[index[i + 1]], vertex[index[i + 2]]);
    if (fabsf(d) < fabsf(minDistance))
    {
      minDistance = d;
    }
  }

  return minDistance;
}

static maths::vec3 gridToLocal(u32 x, u32 y, u32 z, u32 gridWidth, u32 gridHeight, u32 gridDepth, const maths::vec3& aabbMin, const maths::vec3& aabbMax)
{
  maths::vec3 normalized( (f32)(x / (gridWidth - 1.0f)), (f32)(y / (gridHeight - 1.0f)), (f32)(z / (gridDepth - 1.0)) );
  return  vec3( normalized.x * (aabbMax.x - aabbMin.x) + aabbMin.x,
                normalized.y * (aabbMax.y - aabbMin.y) + aabbMin.y,
                normalized.z * (aabbMax.z - aabbMin.z) + aabbMin.z );
}

static void distanceFieldFromMesh(const render::context_t& context, u32 width, u32 height, u32 depth, const bkk::mesh::mesh_t& mesh, render::gpu_buffer_t* buffer)
{
  //Compute distances for an area twice as big as the bounding box of the mesh
  maths::vec3 aabbMinScaled = mesh.aabb_.min_ * 4.0f;
  maths::vec3 aabbMaxScaled = mesh.aabb_.max_ * 4.0f;

  //Read index data from mesh
  uint32_t indexBufferSize = (uint32_t)mesh.indexBuffer_.memory_.size_ - (uint32_t)mesh.indexBuffer_.memory_.offset_;
  uint32_t* index = (uint32_t*)malloc(indexBufferSize);
  memcpy(index, render::gpuBufferMap(context, mesh.indexBuffer_), indexBufferSize);
  gpuBufferUnmap(context, mesh.indexBuffer_);

  //Read vertex data from mesh
  uint32_t vertexBufferSize = (uint32_t)mesh.vertexBuffer_.memory_.size_ - (uint32_t)mesh.vertexBuffer_.memory_.offset_;
  u8* vertex = (u8*)malloc(vertexBufferSize);
  memcpy(vertex, render::gpuBufferMap(context, mesh.vertexBuffer_), vertexBufferSize);
  gpuBufferUnmap(context, mesh.vertexBuffer_);
  vec3* vertexPosition = (vec3*)malloc(sizeof(vec3) * mesh.vertexCount_ );
  for (u32 i(0); i < mesh.vertexCount_; ++i)
  {
    vertexPosition[i] = *(vec3*)(vertex + i*mesh.vertexFormat_.vertexSize_);
  }
  
  //Generate distance field
  f32* data = (f32*)malloc(sizeof(f32) * width * height * depth);
  for (u32 z = 0; z<depth; ++z)
  {
    for (u32 y = 0; y<height; ++y)
    {
      for (u32 x = 0; x<width; ++x)
      {
        float distance = signedDistancePointMesh(gridToLocal(x, y, z, width, height, depth, aabbMinScaled, aabbMaxScaled), index, mesh.indexCount_, vertexPosition, mesh.vertexCount_);
        data[z*width*height + y*width + x] = distance;
      }
    }
  }
  
  //Upload data to the buffer
  struct DistanceFieldBufferData
  {
    mat4 tx;
    u32 width;
    u32 height;
    u32 depth;
    u32 padding;
    vec4 aabbMin;
    vec4 aabbMax;
  };

  DistanceFieldBufferData field;
  field.tx.setIdentity();
  field.width = width;
  field.height = height;
  field.depth = depth;
  field.aabbMin = maths::vec4(aabbMinScaled.x, aabbMinScaled.y, aabbMinScaled.z, 0.0f);
  field.aabbMax = maths::vec4(aabbMaxScaled.x, aabbMaxScaled.y, aabbMaxScaled.z, 0.0f);

  render::gpuBufferCreate(gContext, render::gpu_buffer_usage_e::STORAGE_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    nullptr, sizeof(DistanceFieldBufferData) + sizeof(float) * width * height * depth,
    buffer);

  render::gpuBufferUpdate(gContext, (void*)&field, 0, sizeof(DistanceFieldBufferData), buffer);
  render::gpuBufferUpdate(gContext, data, sizeof(DistanceFieldBufferData), sizeof(float) * width * height * depth, buffer);

  
  free(index);
  free(vertex);
  free(vertexPosition);
  free(data);
}

bool CreateUniformBuffer()
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
  UniformBufferData data;
  data.sampleCount = gSampleCount;
  data.maxBounces = 3;
  data.imageSize = gImageSize;
  data.camera.tx = gCamera.tx_;
  data.camera.verticalFov = (f32)M_PI_2;
  data.camera.focalDistance = 5.0f;
  data.camera.aperture = 0.05f;
 
  //Create uniform buffer
  render::gpuBufferCreate(gContext, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    (void*)&data, sizeof(data),
    &gUbo);
   
  
  return true;
}

void CreateFullscreenQuad( mesh::mesh_t* quad )
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
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  mesh::create(gContext, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, quad);
}

void CreateGraphicsPipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t binding = { render::descriptor_type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_stage_e::FRAGMENT };
  render::descriptorSetLayoutCreate(gContext,1u, &binding,  &descriptorSetLayout);

  //Create pipeline layout
  render::pipelineLayoutCreate(gContext, 1u, &descriptorSetLayout, &gPipelineLayout);

  //Create descriptor pool
  gDescriptorPool = {};
  render::descriptorPoolCreate(gContext, 2u, 1u, 1u, 1u, 1u, &gDescriptorPool);

  //Create descriptor set
  gDescriptorSet.descriptors_.resize(1);
  gDescriptorSet.descriptors_[0].imageDescriptor_ = gTexture.descriptor_;
  render::descriptorSetCreate(gContext, gDescriptorPool, descriptorSetLayout, &gDescriptorSet);

  //Load shaders
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::VERTEX_SHADER, gVertexShaderSource, &gVertexShader);
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &gFragmentShader);

  //Create graphics pipeline
  bkk::render::graphics_pipeline_desc_t pipelineDesc;
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)gContext.swapChain_.imageWidth_, (float)gContext.swapChain_.imageHeight_, 0.0f, 1.0f };
  pipelineDesc.scissorRect_ = { { 0,0 },{ gContext.swapChain_.imageWidth_,gContext.swapChain_.imageHeight_ } };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = false;
  pipelineDesc.depthWriteEnabled_ = false;
  pipelineDesc.vertexShader_ = gVertexShader;
  pipelineDesc.fragmentShader_ = gFragmentShader;
  render::graphicsPipelineCreate(gContext, gContext.swapChain_.renderPass_, gFSQuad.vertexFormat_, gPipelineLayout, pipelineDesc, &gPipeline);
}

void CreateComputePipeline()
{
  //Create descriptor layout
  render::descriptor_set_layout_t descriptorSetLayout;

  std::array< render::descriptor_binding_t, 3> bindings{
    render::descriptor_binding_t{ render::descriptor_type_e::STORAGE_IMAGE,  0, render::descriptor_stage_e::COMPUTE },
    render::descriptor_binding_t{ render::descriptor_type_e::UNIFORM_BUFFER, 1, render::descriptor_stage_e::COMPUTE },
    render::descriptor_binding_t{ render::descriptor_type_e::STORAGE_BUFFER, 2, render::descriptor_stage_e::COMPUTE }
  };

  render::descriptorSetLayoutCreate(gContext, bindings.size(), &bindings[0], &descriptorSetLayout);

  //Create pipeline layout
  render::pipelineLayoutCreate(gContext, 1u, &descriptorSetLayout, &gComputePipelineLayout);

  //Create descriptor set
  gComputeDescriptorSet.descriptors_.resize(3);
  gComputeDescriptorSet.descriptors_[0].imageDescriptor_ = gTexture.descriptor_;
  gComputeDescriptorSet.descriptors_[1].bufferDescriptor_ = gUbo.descriptor_;
  gComputeDescriptorSet.descriptors_[2].bufferDescriptor_ = gDistanceField.descriptor_;
  render::descriptorSetCreate(gContext, gDescriptorPool, descriptorSetLayout, &gComputeDescriptorSet);

  //Create pipeline
  bkk::render::shaderCreateFromGLSL(gContext, bkk::render::shader_t::COMPUTE_SHADER, "../distance-field/distance-field.comp", &gComputeShader);
  gComputePipeline.computeShader_ = gComputeShader;
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

    bkk::render::graphicsPipelineBind(cmdBuffer, gPipeline);
    bkk::render::descriptorSetBindForGraphics(cmdBuffer, gPipelineLayout, 0, &gDescriptorSet, 1u);
    mesh::draw(cmdBuffer, gFSQuad);

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

  bkk::render::computePipelineBind(gComputeCommandBuffer, gComputePipeline);
  bkk::render::descriptorSetBindForCompute(gComputeCommandBuffer, gComputePipelineLayout, 0, &gComputeDescriptorSet, 1u);

  vkCmdDispatch(gComputeCommandBuffer, gImageSize.x / 16, gImageSize.y / 16, 1);

  vkEndCommandBuffer(gComputeCommandBuffer);
}

void Exit()
{
  //Wait for all pending operations to be finished
  render::contextFlush(gContext);

  //Destroy all resources
  render::freeCommandBuffers(gContext, 1, &gComputeCommandBuffer);

  mesh::destroy(gContext, &gFSQuad);
  render::textureDestroy(gContext, &gTexture);
  render::gpuBufferDestroy(gContext, &gUbo);
  render::gpuBufferDestroy(gContext, &gDistanceField);

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
  render::gpuBufferUpdate(gContext, (void*)&gCamera.tx_, offsetof(UniformBufferData, camera), sizeof(mat4), &gUbo);
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
      gCamera.Move(0.0f, -1.0f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_DOWN:
    case 's':
    {
      gCamera.Move(0.0f, 1.0f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_LEFT:
    case 'a':
    {
      gCamera.Move(1.0f, 0.0f);
      UpdateCameraTransform();
      break;
    }
    case window::key_e::KEY_RIGHT:
    case 'd':
    {
      gCamera.Move(-1.0f, 0.0f);
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
  window::create("Distance Field", gImageSize.x, gImageSize.y, &gWindow);

  //Initialize gContext
  render::contextCreate("Distance Field", "", gWindow, 3, &gContext);
  
  CreateFullscreenQuad( &gFSQuad );
  CreateUniformBuffer();
  
  //Create distance field buffer
  bkk::mesh::mesh_t cube = CreateCube(gContext, 1u, 1u, 1u);
  distanceFieldFromMesh(gContext, 50, 50, 50, cube, &gDistanceField);
  mesh::destroy(gContext, &cube);

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
