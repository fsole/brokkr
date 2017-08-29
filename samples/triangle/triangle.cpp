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

bkk::mesh::mesh_t CreateTriangleGeometry(const bkk::render::context_t& context )
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
  attributes[0].format_ = bkk::render::attribute_format_e::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = bkk::render::attribute_format_e::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create( context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh );
  return mesh;
}

void CreatePipeline(const bkk::render::context_t& context, const bkk::mesh::mesh_t& mesh, const bkk::render::shader_t& vertexShader, const bkk::render::shader_t& fragmentShader,
  bkk::render::pipeline_layout_t* layout, bkk::render::graphics_pipeline_t* pipeline )

{
  //Create pipeline layout
  bkk::render::pipelineLayoutCreate( context, 0u, nullptr, layout );

  //Create pipeline
  bkk::render::graphics_pipeline_desc_t pipelineDesc;
  pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f};
  pipelineDesc.scissorRect_ = { {0,0}, {context.swapChain_.imageWidth_,context.swapChain_.imageHeight_} };
  pipelineDesc.blendState_.resize(1);
  pipelineDesc.blendState_[0].colorWriteMask = 0xF;
  pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled_ = false;
  pipelineDesc.depthWriteEnabled_ = false;
  pipelineDesc.vertexShader_ = vertexShader;
  pipelineDesc.fragmentShader_ = fragmentShader;
  bkk::render::graphicsPipelineCreate( context, context.swapChain_.renderPass_, mesh.vertexFormat_, *layout, pipelineDesc, pipeline );
}

void BuildCommandBuffers(const bkk::render::context_t& context, const bkk::mesh::mesh_t& mesh, const bkk::render::graphics_pipeline_t& pipeline )
{
  for( unsigned i(0); i<3; ++i )
  {
    VkCommandBuffer cmdBuffer = bkk::render::beginPresentationCommandBuffer( context, i, nullptr );
    bkk::render::graphicsPipelineBind(cmdBuffer, pipeline);
    bkk::mesh::draw(cmdBuffer, mesh);
    bkk::render::endPresentationCommandBuffer( context, i );
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

  bkk::mesh::mesh_t mesh = CreateTriangleGeometry( context );

  bkk::render::shader_t vertexShader, fragmentShader;
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader);
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader);

  bkk::render::pipeline_layout_t pipelineLayout;
  bkk::render::graphics_pipeline_t pipeline = {};
  CreatePipeline(context, mesh, vertexShader, fragmentShader, &pipelineLayout, &pipeline );
  BuildCommandBuffers(context, mesh, pipeline);
  
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
        BuildCommandBuffers(context, mesh, pipeline);
      }
    }

    //Render next image
    bkk::render::presentNextImage( &context );
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