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

#include "core/render.h"
#include "core/window.h"
#include "core/mesh.h"

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;

  layout(location = 0) out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition,1.0);
    uv = aTexCoord;
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec2 uv;
  layout(location = 0) out vec4 color;

  void main(void)
  {
    color = vec4(uv,0.0,1.0);
  }
)";

bkk::core::mesh::mesh_t createTriangleGeometry(const bkk::core::render::context_t& context )
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: In Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[3] = { { { -0.5f, +0.5f, +0.0f }, { 0.0f, 0.0f } },
                                      { { +0.5f, +0.5f, +0.0f }, { 1.0f, 0.0f } },
                                      { { +0.0f, -0.5f, +0.0f }, { 0.5f, 1.0f } }
                                    };

  static const uint32_t indices[3] = {0,1,2};

  static bkk::core::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::core::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[0].instanced_ = false;
  attributes[1].format_ = bkk::core::render::vertex_attribute_t::format::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);
  attributes[1].instanced_ = false;

  bkk::core::mesh::mesh_t mesh;
  bkk::core::mesh::create( context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh );
  return mesh;
}

void buildCommandBuffers(const bkk::core::render::context_t& context, const bkk::core::mesh::mesh_t& mesh, const bkk::core::render::graphics_pipeline_t& pipeline )
{
  const bkk::core::render::command_buffer_t* commandBuffers;
  uint32_t count = bkk::core::render::getPresentationCommandBuffers(context, &commandBuffers);
  for (uint32_t i(0); i<count; ++i)
  {
    bkk::core::render::beginPresentationCommandBuffer(context, i, nullptr);
    bkk::core::render::graphicsPipelineBind(commandBuffers[i], pipeline);
    bkk::core::mesh::draw(commandBuffers[i], mesh);
    bkk::core::render::endPresentationCommandBuffer(context, i);
  }
}

int main()
{
  //Create a window
  bkk::core::window::window_t window;
  bkk::core::window::create( "Hello Triangle", 400u, 400u, &window );

  //Create a context
  bkk::core::render::context_t context;
  bkk::core::render::contextCreate( "Hello triangle", "", window, 3, &context );

  //Create a mesh
  bkk::core::mesh::mesh_t mesh = createTriangleGeometry( context );
  
  //Create pipeline layout
  bkk::core::render::pipeline_layout_t pipelineLayout;
  bkk::core::render::pipelineLayoutCreate(context, nullptr, 0u, nullptr, 0u, &pipelineLayout);

  //Load shaders  
  bkk::core::render::shader_t vertexShader, fragmentShader;
  bkk::core::render::shaderCreateFromGLSLSource(context, bkk::core::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader);
  bkk::core::render::shaderCreateFromGLSLSource(context, bkk::core::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader);  
  
  //Create pipeline
  bkk::core::render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
  pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = false;
  pipelineDesc.depthWriteEnabled_ = false;
  pipelineDesc.vertexShader_ = vertexShader;
  pipelineDesc.fragmentShader_ = fragmentShader;

  bkk::core::render::graphics_pipeline_t pipeline;
  bkk::core::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh.vertexFormat_, pipelineLayout, pipelineDesc, &pipeline);

  //Build command buffers
  buildCommandBuffers(context, mesh, pipeline);
  
  bool quit = false;
  while( !quit )
  {
    bkk::core::window::event_t* event = nullptr;
    while( (event = bkk::core::window::getNextEvent( &window )) )
    {
      if( event->type_ == bkk::core::window::EVENT_QUIT )
      {
        quit = true;
      }
      else if( event->type_ == bkk::core::window::EVENT_RESIZE )
      {
        bkk::core::window::event_resize_t* resizeEvent = (bkk::core::window::event_resize_t*)event;
        bkk::core::render::swapchainResize( &context, resizeEvent->width_, resizeEvent->height_ );
        buildCommandBuffers(context, mesh, pipeline);
      }
    }

    bkk::core::render::presentFrame( &context );
  }

  //Wait for all pending operations to be finished
  bkk::core::render::contextFlush( context );

  //Clean-up all resources
  bkk::core::render::graphicsPipelineDestroy( context, &pipeline );
  bkk::core::render::pipelineLayoutDestroy( context, &pipelineLayout );
  bkk::core::mesh::destroy( context, &mesh );
  bkk::core::render::shaderDestroy(context, &vertexShader);
  bkk::core::render::shaderDestroy(context, &fragmentShader);
  bkk::core::render::contextDestroy( &context );

  //Close window
  bkk::core::window::destroy( &window );

  return 0;
}