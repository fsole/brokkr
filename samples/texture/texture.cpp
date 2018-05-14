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
#include "image.h"
#include "mesh.h"
#include "camera.h"

static const char* gVertexShaderSource = R"(
  #version 440 core

  layout(location = 0) in vec3 aPosition;
  layout(location = 1) in vec2 aTexCoord;

  out vec2 uv;

  void main(void)
  {
    gl_Position = vec4(aPosition,1.0);
    uv = aTexCoord;
  }
)";

static const char* gFragmentShaderSource = R"(
  #version 440 core

  in vec2 uv;
  layout(location = 0) out vec4 color;

  layout (binding = 0) uniform sampler2D uTexture;

  void main(void)
  {
    color = texture(uTexture, uv);
  }
)";


bkk::render::texture_t createTexture(const bkk::render::context_t& context)
{
  bkk::render::texture_t texture;
  bkk::image::image2D_t image = {};
  if (!bkk::image::load("../resources/brokkr.png", false, &image))
  {
    printf("Error loading texture\n");
  }
  else
  {
    //Create the texture
    bkk::render::texture2DCreate(context, &image, 1, bkk::render::texture_sampler_t(), &texture);
    bkk::image::unload(&image);
  }

  return texture;
}

void buildCommandBuffers(const bkk::render::context_t& context,const bkk::mesh::mesh_t& mesh,
                         bkk::render::descriptor_set_t* descriptorSet,
                         bkk::render::pipeline_layout_t* layout, bkk::render::graphics_pipeline_t* pipeline)
{
  const VkCommandBuffer* commandBuffers;
  uint32_t count = bkk::render::getPresentationCommandBuffers(context, &commandBuffers);
  for (uint32_t i(0); i<count; ++i)
  {
    bkk::render::beginPresentationCommandBuffer(context, i, nullptr);
    bkk::render::graphicsPipelineBind(commandBuffers[i], *pipeline);
    bkk::render::descriptorSetBindForGraphics(commandBuffers[i], *layout, 0, descriptorSet, 1u);
    bkk::mesh::draw(commandBuffers[i], mesh);
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
  bkk::mesh::mesh_t mesh = bkk::mesh::fullScreenQuad(context);
  bkk::render::texture_t texture = createTexture(context);

  //Create descriptor pool
  bkk::render::descriptor_pool_t descriptorPool;
  bkk::render::descriptorPoolCreate( context, 1u,
                                bkk::render::combined_image_sampler_count(1u),
                                bkk::render::uniform_buffer_count(0u),
                                bkk::render::storage_buffer_count(0u),
                                bkk::render::storage_image_count(0u),
                                &descriptorPool);

  //Create descriptor layout
  bkk::render::descriptor_set_layout_t descriptorSetLayout;
  bkk::render::descriptor_binding_t binding = { bkk::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, bkk::render::descriptor_t::stage::FRAGMENT };
  bkk::render::descriptorSetLayoutCreate(context, &binding, 1u, &descriptorSetLayout);

  //Create pipeline layout
  bkk::render::pipeline_layout_t pipelineLayout;
  bkk::render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, nullptr, 0u, &pipelineLayout);

  //Create descriptor set
  bkk::render::descriptor_set_t descriptorSet;
  bkk::render::descriptor_t descriptor = bkk::render::getDescriptor(texture);
  bkk::render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &descriptor, &descriptorSet);

  //Load shaders
  bkk::render::shader_t vertexShader, fragmentShader;
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, gVertexShaderSource, &vertexShader);
  bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, gFragmentShaderSource, &fragmentShader);

  //Create pipeline
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

  bkk::render::graphics_pipeline_t pipeline;
  bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, mesh.vertexFormat_, pipelineLayout, pipelineDesc, &pipeline);

  //Build command buffers
  buildCommandBuffers(context, mesh, &descriptorSet, &pipelineLayout, &pipeline);

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
        buildCommandBuffers(context, mesh, &descriptorSet, &pipelineLayout, &pipeline);
      }
    }

    bkk::render::presentFrame(&context);
  }

  //Wait for all pending operations to be finished
  bkk::render::contextFlush(context);

  //Destroy all resources
  bkk::mesh::destroy(context, &mesh);
  bkk::render::textureDestroy(context, &texture);

  bkk::render::shaderDestroy(context, &vertexShader);
  bkk::render::shaderDestroy(context, &fragmentShader);

  bkk::render::graphicsPipelineDestroy(context, &pipeline);
  bkk::render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  bkk::render::descriptorSetDestroy(context, &descriptorSet);
  bkk::render::descriptorPoolDestroy(context, &descriptorPool);
  bkk::render::pipelineLayoutDestroy(context, &pipelineLayout);

  bkk::render::contextDestroy(&context);

  //Close window
  bkk::window::destroy(&window);

  return 0;
}
