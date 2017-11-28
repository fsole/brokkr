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
#include "mesh.h"

static const char* gVertexShaderSource = {
  "#version 440 core\n \
  layout(location = 0) in vec3 aPosition;\n \
  layout(location = 1) in vec2 aTexCoord;\n \
  out vec2 uv;\n \
  void main(void)\n \
  {\n \
    gl_Position = vec4(aPosition,1.0);\n \
    uv = aTexCoord;\n \
  }\n"
};


static const char* gFragmentShaderSource = {
  "#version 440 core\n \
  in vec2 uv;\n  \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    color = vec4(uv,0.0,1.0);\n \
  }\n"
};

bkk::mesh::mesh_t createTriangleGeometry(const bkk::render::context_t& context )
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

  static bkk::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = bkk::render::vertex_attribute_t::format::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create( context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh );
  return mesh;
}

void buildCommandBuffers(const bkk::render::context_t& context, const bkk::mesh::mesh_t& mesh, const bkk::render::graphics_pipeline_t& pipeline )
{
  const VkCommandBuffer* commandBuffers;
  uint32_t count = bkk::render::getPresentationCommandBuffers(context, &commandBuffers);
  for (uint32_t i(0); i<count; ++i)
  {
    bkk::render::beginPresentationCommandBuffer(context, i, nullptr);
    bkk::render::graphicsPipelineBind(commandBuffers[i], pipeline);
    bkk::mesh::draw(commandBuffers[i], mesh);
    bkk::render::endPresentationCommandBuffer(context, i);
  }
}

int main()
{
  //Create a window
  bkk::window::window_t window;
  bkk::window::create( "Hello Triangle", 400u, 400u, &window );

  //Create a context
  bkk::render::context_t context;
  bkk::render::contextCreate( "Hello triangle", "", window, 3, &context );

  //Create a mesh
  bkk::mesh::mesh_t mesh = createTriangleGeometry( context );
  
  //Create pipeline layout
  bkk::render::pipeline_layout_t pipelineLayout;
  bkk::render::pipelineLayoutCreate(context, nullptr, 0u, &pipelineLayout);

  //Create pipeline
  bkk::render::graphics_pipeline_t pipeline;
  bkk::render::shader_t vertexShader, fragmentShader;
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader);
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader);  
  bkk::render::graphics_pipeline_t::description_t pipelineDesc = {};
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
  bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh.vertexFormat_, pipelineLayout, pipelineDesc, &pipeline);

  //Build command buffers
  buildCommandBuffers(context, mesh, pipeline);
  
  bool quit = false;
  while( !quit )
  {
    bkk::window::event_t* event = nullptr;
    while( (event = bkk::window::getNextEvent( &window )) )
    {
      if( event->type_ == bkk::window::EVENT_QUIT )
      {
        quit = true;
      }
      else if( event->type_ == bkk::window::EVENT_RESIZE )
      {
        bkk::window::event_resize_t* resizeEvent = (bkk::window::event_resize_t*)event;
        bkk::render::swapchainResize( &context, resizeEvent->width_, resizeEvent->height_ );
        buildCommandBuffers(context, mesh, pipeline);
      }
    }

    bkk::render::presentFrame( &context );
  }

  //Wait for all pending operations to be finished
  bkk::render::contextFlush( context );

  //Clean-up all resources
  bkk::render::graphicsPipelineDestroy( context, &pipeline );
  bkk::render::pipelineLayoutDestroy( context, &pipelineLayout );
  bkk::mesh::destroy( context, &mesh );
  bkk::render::shaderDestroy(context, &vertexShader);
  bkk::render::shaderDestroy(context, &fragmentShader);
  bkk::render::contextDestroy( &context );

  //Close window
  bkk::window::destroy( &window );

  return 0;
}