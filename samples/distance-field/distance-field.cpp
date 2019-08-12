/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/


// This sample is work in progress.
#include "framework/camera.h"

#include "core/render.h"
#include "core/window.h"
#include "core/image.h"
#include "core/mesh.h"

using namespace bkk;
using namespace bkk::core;
using namespace bkk::core::maths;

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;
  layout(location = 0)out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition, 1.0);
    uv = vec2(aTexCoord.x, -aTexCoord.y + 1.0);
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  layout(binding = 0) uniform sampler2D uTexture;
  layout(location = 0) in vec2 uv;  
  layout(location = 0) out vec4 result;

  void main(void)
  {
    vec4 texColor = texture(uTexture, uv);
    vec3 color = texColor.rgb;
    color = pow(color, vec3(1.0 / 2.2));
    result = vec4(color, 1.0);
  }
)";

struct camera_t
{
  maths::mat4 tx;
  f32 verticalFov;
  f32 focalDistance;
  f32 aperture;
};

struct buffer_data_t
{
  u32 sampleCount;
  u32 maxBounces;
  maths::uvec2 imageSize;
  camera_t camera;
};

static render::context_t gContext;
static window::window_t gWindow;
static render::texture_t gTexture;
static mesh::mesh_t gFSQuad;

static render::descriptor_pool_t gDescriptorPool;

static render::pipeline_layout_t gPipelineLayout;
static render::descriptor_set_layout_t gDescriptorSetLayout;
static render::descriptor_set_t gDescriptorSet;
static render::graphics_pipeline_t gPipeline;


static render::pipeline_layout_t gComputePipelineLayout;
static render::descriptor_set_layout_t gComputeDescriptorSetLayout;
static render::descriptor_set_t gComputeDescriptorSet;
static render::compute_pipeline_t gComputePipeline;
static render::gpu_buffer_t gUbo;
static render::gpu_buffer_t gDistanceField;

static render::command_buffer_t gComputeCommandBuffer;
static render::shader_t gVertexShader;
static render::shader_t gFragmentShader;
static render::shader_t gComputeShader;

static framework::free_camera_controller_t gCamera;
static maths::vec2 gMousePosition = vec2(0.0f, 0.0f);
static bool gMouseButtonPressed = false;

static maths::uvec2 gImageSize = { 1200u,800u };
static u32 gSampleCount = 0u;


static mesh::mesh_t createCube(const render::context_t& context, u32 width, u32 height, u32 depth)
{
  float hw = width / 2.0f;
  float hh = height / 2.0f;
  float hd = depth / 2.0f;

  vec3 vertices[8] = { { -hw,-hh,hd },{ hw,-hh,hd },{ -hw,hh,hd },{ hw,hh,hd },{ -hw,-hh,-hd },{ hw,-hh,-hd },{ -hw,hh,-hd },{ hw,hh,-hd } };
  u32 indices[36] = { 0,1,2, 1,3,2,  1,5,3, 5,7,3,
                      4,0,6, 0,2,6,  5,4,7, 4,6,7,
                      2,3,6, 3,7,6,  4,5,0, 5,1,0 };


  static render::vertex_attribute_t attributes[1];
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = sizeof(vec3);
  attributes[0].instanced = false;

  mesh::mesh_t mesh;
  mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 1, nullptr, &mesh);

  mesh.aabb.min = vec3(-hw, -hh, -hd);
  mesh.aabb.max = vec3(hw, hh, hd);
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

  return sign * length(v);
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

static void distanceFieldFromMesh(const render::context_t& context, u32 width, u32 height, u32 depth, const mesh::mesh_t& mesh, render::gpu_buffer_t* buffer)
{
  //Compute distances for an area twice as big as the bounding box of the mesh
  maths::vec3 aabbMinScaled = mesh.aabb.min * 4.0f;
  maths::vec3 aabbMaxScaled = mesh.aabb.max * 4.0f;

  //Read index data from mesh
  uint32_t indexBufferSize = (uint32_t)mesh.indexBuffer.memory.size - (uint32_t)mesh.indexBuffer.memory.offset;
  uint32_t* index = (uint32_t*)malloc(indexBufferSize);
  memcpy(index, render::gpuBufferMap(context, mesh.indexBuffer), indexBufferSize);
  gpuBufferUnmap(context, mesh.indexBuffer);

  //Read vertex data from mesh
  uint32_t vertexBufferSize = (uint32_t)mesh.vertexBuffer.memory.size - (uint32_t)mesh.vertexBuffer.memory.offset;
  uint8_t* vertex = (uint8_t*)malloc(vertexBufferSize);
  memcpy(vertex, render::gpuBufferMap(context, mesh.vertexBuffer), vertexBufferSize);
  gpuBufferUnmap(context, mesh.vertexBuffer);
  vec3* vertexPosition = (vec3*)malloc(sizeof(vec3) * mesh.vertexCount );
  for (u32 i(0); i < mesh.vertexCount; ++i)
  {
    vertexPosition[i] = *(vec3*)(vertex + i*mesh.vertexFormat.vertexSize);
  }
  
  //Generate distance field
  f32* data = (f32*)malloc(sizeof(f32) * width * height * depth);
  for (u32 z = 0; z<depth; ++z)
  {
    for (u32 y = 0; y<height; ++y)
    {
      for (u32 x = 0; x<width; ++x)
      {
        float distance = signedDistancePointMesh(gridToLocal(x, y, z, width, height, depth, aabbMinScaled, aabbMaxScaled), index, mesh.indexCount, vertexPosition, mesh.vertexCount);
        data[z*width*height + y*width + x] = distance;
      }
    }
  }
  
  //Upload data to the buffer
  struct distance_field_buffer_data_t
  {
    mat4 tx;
    u32 width;
    u32 height;
    u32 depth;
    u32 padding;
    vec4 aabbMin;
    vec4 aabbMax;
  };

  distance_field_buffer_data_t field;
  field.tx.setIdentity();
  field.width = width;
  field.height = height;
  field.depth = depth;
  field.aabbMin = maths::vec4(aabbMinScaled.x, aabbMinScaled.y, aabbMinScaled.z, 0.0f);
  field.aabbMax = maths::vec4(aabbMaxScaled.x, aabbMaxScaled.y, aabbMaxScaled.z, 0.0f);

  render::gpuBufferCreate(gContext, render::gpu_buffer_t::STORAGE_BUFFER,
                          render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                          nullptr, sizeof(distance_field_buffer_data_t) + sizeof(float) * width * height * depth,
                          nullptr, buffer);

  render::gpuBufferUpdate(gContext, (void*)&field, 0, sizeof(distance_field_buffer_data_t), buffer);
  render::gpuBufferUpdate(gContext, data, sizeof(distance_field_buffer_data_t), sizeof(float) * width * height * depth, buffer);

  
  free(index);
  free(vertex);
  free(vertexPosition);
  free(data);
}

bool createUniformBuffer()
{
  //Create the texture
  render::texture2DCreate(gContext, gImageSize.x, gImageSize.y, 1u, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, render::texture_sampler_t(), &gTexture);
  render::textureChangeLayoutNow(gContext, VK_IMAGE_LAYOUT_GENERAL, &gTexture);

  //Create data to be passed to the gpu
  buffer_data_t data;
  data.sampleCount = gSampleCount;
  data.maxBounces = 3;
  data.imageSize = gImageSize;
  data.camera.tx = gCamera.getWorldMatrix();
  data.camera.verticalFov = (f32)PI_2;
  data.camera.focalDistance = 5.0f;
  data.camera.aperture = 0.05f;
 
  //Create uniform buffer
  render::gpuBufferCreate(gContext, render::gpu_buffer_t::UNIFORM_BUFFER,
                          render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                          (void*)&data, sizeof(data),
                          nullptr, &gUbo);
   
  
  return true;
}

void createFullscreenQuad( mesh::mesh_t* quad )
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
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = sizeof(Vertex);
  attributes[0].instanced = false;
  attributes[1].format = render::vertex_attribute_t::format_e::VEC2;;
  attributes[1].offset = offsetof(Vertex, uv);
  attributes[1].stride = sizeof(Vertex);
  attributes[1].instanced = false;

  mesh::create(gContext, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, quad);
}

void createGraphicsPipeline()
{
  //Create descriptor layout
  render::descriptor_binding_t binding = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, render::descriptor_t::stage_e::FRAGMENT };
  render::descriptorSetLayoutCreate(gContext, &binding, 1u, &gDescriptorSetLayout);

  //Create pipeline layout
  render::pipelineLayoutCreate(gContext, &gDescriptorSetLayout, 1u, nullptr, 0u, &gPipelineLayout);

  //Create descriptor pool
  render::descriptorPoolCreate(gContext, 2u,
    render::combined_image_sampler_count(1u),
    render::uniform_buffer_count(1u),
    render::storage_buffer_count(1u),
    render::storage_image_count(1u),
    &gDescriptorPool);

  //Create descriptor set
  render::descriptor_t descriptor = render::getDescriptor(gTexture);
  render::descriptorSetCreate(gContext, gDescriptorPool, gDescriptorSetLayout, &descriptor, &gDescriptorSet);

  //Load shaders
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::VERTEX_SHADER, gVertexShaderSource, &gVertexShader);
  render::shaderCreateFromGLSLSource(gContext, render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &gFragmentShader);

  //Create graphics pipeline
  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)gContext.swapChain.imageWidth, (float)gContext.swapChain.imageHeight, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ gContext.swapChain.imageWidth, gContext.swapChain.imageHeight } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = gVertexShader;
  pipelineDesc.fragmentShader = gFragmentShader;
  render::graphicsPipelineCreate(gContext, gContext.swapChain.renderPass, 0u, gFSQuad.vertexFormat, gPipelineLayout, pipelineDesc, &gPipeline);
}

void createComputePipeline()
{
  //Create descriptor layout
    render::descriptor_binding_t bindings[3] = {  
      render::descriptor_binding_t{ render::descriptor_t::type_e::STORAGE_IMAGE,  0, render::descriptor_t::stage_e::COMPUTE },
      render::descriptor_binding_t{ render::descriptor_t::type_e::UNIFORM_BUFFER, 1, render::descriptor_t::stage_e::COMPUTE },
      render::descriptor_binding_t{ render::descriptor_t::type_e::STORAGE_BUFFER, 2, render::descriptor_t::stage_e::COMPUTE }
    };

  render::descriptorSetLayoutCreate(gContext, bindings, 3u, &gComputeDescriptorSetLayout);

  //Create pipeline layout
  render::pipelineLayoutCreate(gContext, &gComputeDescriptorSetLayout, 1u, nullptr, 0u, &gComputePipelineLayout);

  //Create descriptor set
  render::descriptor_t descriptors[3] = { render::getDescriptor(gTexture), render::getDescriptor(gUbo), render::getDescriptor(gDistanceField) };
  render::descriptorSetCreate(gContext, gDescriptorPool, gComputeDescriptorSetLayout, descriptors, &gComputeDescriptorSet);

  //Create pipeline
  render::shaderCreateFromGLSL(gContext, render::shader_t::COMPUTE_SHADER, "../distance-field/distance-field.comp", &gComputeShader);  
  render::computePipelineCreate(gContext, gComputePipelineLayout, gComputeShader, &gComputePipeline);
}

void createPipelines()
{
  createGraphicsPipeline();
  createComputePipeline();
}

void buildCommandBuffers()
{
  const render::command_buffer_t* commandBuffers;
  uint32_t count = render::getPresentationCommandBuffers(gContext, &commandBuffers);
  for (unsigned i(0); i<count; ++i)
  {
    render::beginPresentationCommandBuffer(gContext, i, nullptr);
    render::graphicsPipelineBind(commandBuffers[i], gPipeline);
    render::descriptorSetBind(commandBuffers[i], gPipelineLayout, 0, &gDescriptorSet, 1u);
    mesh::draw(commandBuffers[i], gFSQuad);
    render::endPresentationCommandBuffer(gContext, i);
  }
}

void buildComputeCommandBuffer()
{
  //Build compute command buffer
  render::commandBufferCreate(gContext, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::COMPUTE, VK_NULL_HANDLE, &gComputeCommandBuffer);

  render::commandBufferBegin(gContext, gComputeCommandBuffer);
  render::computePipelineBind(gComputeCommandBuffer, gComputePipeline);
  render::descriptorSetBind(gComputeCommandBuffer, gComputePipelineLayout, 0, &gComputeDescriptorSet, 1u);
  render::computeDispatch(gComputeCommandBuffer, gImageSize.x / 16, gImageSize.y / 16, 1);
  render::commandBufferEnd(gComputeCommandBuffer);
}

void exit()
{
  //Wait for all pending operations to be finished
  render::contextFlush(gContext);

  //Destroy all resources
  render::commandBufferDestroy(gContext, &gComputeCommandBuffer);

  mesh::destroy(gContext, &gFSQuad);
  render::textureDestroy(gContext, &gTexture);
  render::gpuBufferDestroy(gContext, nullptr, &gUbo);
  render::gpuBufferDestroy(gContext, nullptr, &gDistanceField);

  render::shaderDestroy(gContext, &gVertexShader);
  render::shaderDestroy(gContext, &gFragmentShader);
  render::shaderDestroy(gContext, &gComputeShader);

  render::graphicsPipelineDestroy(gContext, &gPipeline);
  render::descriptorSetLayoutDestroy(gContext, &gDescriptorSetLayout);
  render::descriptorSetDestroy(gContext, &gDescriptorSet);
  render::pipelineLayoutDestroy(gContext, &gPipelineLayout);

  render::computePipelineDestroy(gContext, &gComputePipeline);
  render::descriptorSetLayoutDestroy(gContext, &gComputeDescriptorSetLayout);
  render::descriptorSetDestroy(gContext, &gComputeDescriptorSet);
  render::pipelineLayoutDestroy(gContext, &gComputePipelineLayout);

  render::descriptorPoolDestroy(gContext, &gDescriptorPool);

  render::contextDestroy(&gContext);

  //Close window
  window::destroy(&gWindow);
}

void renderFrame()
{
  ++gSampleCount;

  render::presentFrame(&gContext);

  //Submit compute command buffer
  render::commandBufferSubmit(gContext, gComputeCommandBuffer);
  vkQueueWaitIdle(gContext.computeQueue.handle);
}

void updateCameraTransform()
{
  render::gpuBufferUpdate(gContext, (void*)&gCamera.getWorldMatrix(), offsetof(buffer_data_t, camera), sizeof(mat4), &gUbo);
  gSampleCount = 0;
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
      gCamera.Move(0.0f, -0.5f);
      updateCameraTransform();
      break;
    }
    case window::key_e::KEY_DOWN:
    case 's':
    {
      gCamera.Move(0.0f, 0.5f);
      updateCameraTransform();
      break;
    }
    case window::key_e::KEY_LEFT:
    case 'a':
    {
      gCamera.Move(-0.5f, 0.0f);
      updateCameraTransform();
      break;
    }
    case window::key_e::KEY_RIGHT:
    case 'd':
    {
      gCamera.Move(0.5f, 0.0f);
      updateCameraTransform();
      break;
    }
    default:
      break;
    }
  }
}

void onMouseButton(window::mouse_button_e button, uint32_t x, uint32_t y, bool pressed)
{
  gMouseButtonPressed = pressed;
  gMousePosition.x = (f32)x;
  gMousePosition.y = (f32)y;
}

void onMouseMove(uint32_t x, uint32_t y)
{
  if (gMouseButtonPressed)
  {
    gCamera.Rotate(x - gMousePosition.x, y - gMousePosition.y);    
    updateCameraTransform();
  }

  gMousePosition.x = (f32)x;
  gMousePosition.y = (f32)y;
}

int main()
{
  //Create a window
  window::create("Distance Field", gImageSize.x, gImageSize.y, &gWindow);

  //Initialize gContext
  render::contextCreate("Distance Field", "", gWindow, 3, &gContext);
  
  gFSQuad = mesh::fullScreenQuad(gContext);
  gCamera.setPosition( vec3(0.0f, 0.0f, 5.0f) );
  gCamera.Update();

  createUniformBuffer();
  
  //Create distance field buffer
  mesh::mesh_t cube = createCube(gContext, 1u, 1u, 1u);
  distanceFieldFromMesh(gContext, 50, 50, 50, cube, &gDistanceField);
  mesh::destroy(gContext, &cube);

  createPipelines();
  buildCommandBuffers();
  buildComputeCommandBuffer();

  bool quit = false;
  while (!quit)
  {
    window::event_t* event = nullptr;
    while ((event = window::getNextEvent(&gWindow)))
    {
      switch (event->type)
      {
      case window::EVENT_QUIT:
      {
        quit = true;
        break;
      }
      case window::EVENT_RESIZE:
      {
        window::event_resize_t* resizeEvent = (window::event_resize_t*)event;
        render::swapchainResize(&gContext, resizeEvent->width, resizeEvent->height);
        buildCommandBuffers();
        break;
      }
      case window::EVENT_KEY:
      {
        window::event_key_t* keyEvent = (window::event_key_t*)event;
        onKeyEvent(keyEvent->keyCode, keyEvent->pressed);
        break;
      }
      case window::EVENT_MOUSE_BUTTON:
      {
        window::event_mouse_button_t* buttonEvent = (window::event_mouse_button_t*)event;
        onMouseButton(buttonEvent->button, buttonEvent->x, buttonEvent->y, buttonEvent->pressed);
        break;
      }
      case window::EVENT_MOUSE_MOVE:
      {
        window::event_mouse_move_t* moveEvent = (window::event_mouse_move_t*)event;
        onMouseMove(moveEvent->x, moveEvent->y);
        break;
      }
      default:
        break;
      }
    }

    renderFrame();
  }


  exit();

  return 0;
}
