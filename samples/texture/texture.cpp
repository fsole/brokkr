
#include "render.h"
#include "window.h"
#include "mesh.h"

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
  layout (binding = 0) uniform sampler2D uTexture;\n \
  layout(location = 0) out vec4 color;\n \
  void main(void)\n \
  {\n \
    color = texture(uTexture, uv);\n \
  }\n"
};

bkk::mesh::mesh_t CreateQuadGeometry(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: IN Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { { { -1.0f, 1.0f, 0.0f },{ 0.0f, 0.0f } },
                                     { { 1.0f,  1.0f, 0.0f },{ 1.0f, 0.0f } },
                                     { { 1.0f, -1.0f, 0.0f },{ 1.0f, 1.0f } },
                                     { { -1.0f,-1.0f, 1.0f },{ 0.0f, 1.0f } }
  };

  static const uint32_t indices[] = { 0,1,2,0,2,3 };


  static bkk::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::render::attribute_format_e::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[1].format_ = bkk::render::attribute_format_e::VEC2;;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, &mesh);
  return mesh;
}

bkk::render::texture_t CreateTexture(const bkk::render::context_t& context)
{
  bkk::render::texture_t texture;
  bkk::image::image2D_t image = {};
  if (!bkk::image::load("../resources/brokkr.png", &image))
  {
    printf("Error loading texture\n");
  }
  else
  {
    //Create the texture
    bkk::render::texture_sampler_t sampler = {};
    sampler.minification_ = bkk::render::filter_mode_e::LINEAR;
    sampler.magnification_ = bkk::render::filter_mode_e::LINEAR;
    sampler.wrapU_ = bkk::render::wrap_mode_e::CLAMP_TO_EDGE;
    sampler.wrapV_ = bkk::render::wrap_mode_e::CLAMP_TO_EDGE;
    bkk::render::texture2DCreate(context, &image, 1, sampler, &texture);
    bkk::render::textureChangeLayout(context, context.initializationCmdBuffer_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, &texture);
    bkk::render::initResources(context);
    bkk::image::unload(&image);
  }

  return texture;
}

void CreatePipeline(const bkk::render::context_t& context, const bkk::render::vertex_format_t& vertexFormat, const bkk::render::shader_t& vertexShader, const bkk::render::shader_t& fragmentShader,
                    const bkk::render::pipeline_layout_t& layout, bkk::render::graphics_pipeline_t* pipeline)
{
  //Create pipeline
  pipeline->viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
  pipeline->scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
  pipeline->blendState_.resize(1);
  pipeline->blendState_[0].colorWriteMask = 0xF;
  pipeline->blendState_[0].blendEnable = VK_FALSE;
  pipeline->cullMode_ = VK_CULL_MODE_BACK_BIT;
  pipeline->depthTestEnabled_ = false;
  pipeline->depthWriteEnabled_ = false;
  pipeline->vertexShader_ = vertexShader;
  pipeline->fragmentShader_ = fragmentShader;
  bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, vertexFormat, layout, pipeline);
}

void BuildCommandBuffers(const bkk::render::context_t& context,const bkk::mesh::mesh_t& mesh,
  bkk::render::pipeline_layout_t* layout, bkk::render::descriptor_set_t* descriptorSet,
  bkk::render::graphics_pipeline_t* pipeline)
{
  for (unsigned i(0); i<3; ++i)
  {
    VkCommandBuffer cmdBuffer = bkk::render::beginPresentationCommandBuffer(context, i, nullptr);
    bkk::render::graphicsPipelineBind(cmdBuffer, *pipeline);
    bkk::render::descriptorSetBindForGraphics(cmdBuffer, *layout, 0, descriptorSet, 1u);
    bkk::mesh::draw(cmdBuffer, mesh);
    bkk::render::endPresentationCommandBuffer(context, i);
  }
}

int main()
{
  //Create a window
  bkk::window::window_t window;
  bkk::window::create("Textured Quad", 400u, 400u, &window);

  //Initialize context
  bkk::render::context_t context;
  bkk::render::contextCreate("Textured Quad", "", window, 3, &context);

  //Create a quad and a texture
  bkk::mesh::mesh_t mesh = CreateQuadGeometry(context);
  bkk::render::texture_t texture = CreateTexture(context);

  //Create descriptor layout
  bkk::render::descriptor_set_layout_t descriptorSetLayout;
  descriptorSetLayout.bindings_.push_back({ bkk::render::descriptor_type_e::COMBINED_IMAGE_SAMPLER, 0, bkk::render::descriptor_stage_e::FRAGMENT });
  bkk::render::descriptorSetLayoutCreate(context, &descriptorSetLayout);

  //Create pipeline layout
  bkk::render::pipeline_layout_t pipelineLayout;
  pipelineLayout.descriptorSetLayout_.push_back(descriptorSetLayout);
  bkk::render::pipelineLayoutCreate(context, &pipelineLayout);

  //Create descriptor pool
  bkk::render::descriptor_pool_t descriptorPool = {};
  descriptorPool.combinedImageSamplers_ = 1u;
  descriptorPool.descriptorSets_ = 1u;
  bkk::render::descriptorPoolCreate(context, &descriptorPool);

  //Create descriptor set
  bkk::render::descriptor_set_t descriptorSet;
  descriptorSet.descriptors_.resize(1);
  descriptorSet.descriptors_[0].imageDescriptor_ = texture.descriptor_;
  bkk::render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &descriptorSet);

  //Load shaders
  bkk::render::shader_t vertexShader, fragmentShader;
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader);
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader);

  bkk::render::graphics_pipeline_t pipeline = {};
  CreatePipeline(context, mesh.vertexFormat_, vertexShader, fragmentShader, pipelineLayout, &pipeline);
  BuildCommandBuffers(context, mesh, &pipelineLayout, &descriptorSet, &pipeline);

  bool quit = false;
  while (!quit)
  {
    bkk::window::event_t* event = nullptr;
    while ((event = bkk::window::getNextEvent(&window)))
    {
      if (event->type_ == bkk::window::EVENT_QUIT)
      {
        quit = true;
      }
      else if (event->type_ == bkk::window::EVENT_RESIZE)
      {
        bkk::window::event_resize_t* resizeEvent = (bkk::window::event_resize_t*)event;
        swapchainResize(&context, resizeEvent->width_, resizeEvent->height_);
        BuildCommandBuffers(context, mesh, &pipelineLayout, &descriptorSet, &pipeline);
      }
    }

    //Render next image
    bkk::render::presentNextImage(&context);
  }

  //Wait for all pending operations to be finished
  bkk::render::contextFlush(context);

  //Destroy all resources
  bkk::mesh::destroy(context, &mesh);
  bkk::render::textureDestroy(context, &texture);

  bkk::render::shaderDestroy(context, &vertexShader);
  bkk::render::shaderDestroy(context, &fragmentShader);

  bkk::render::graphicsPipelineDestroy(context, &pipeline);
  bkk::render::descriptorSetDestroy(context, &descriptorSet);
  bkk::render::descriptorPoolDestroy(context, &descriptorPool);
  bkk::render::pipelineLayoutDestroy(context, &pipelineLayout);

  bkk::render::contextDestroy(&context);

  //Close window
  bkk::window::destroy(&window);

  return 0;
}
