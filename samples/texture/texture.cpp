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

#include <stdio.h>

#include "core/render.h"
#include "core/window.h"
#include "core/mesh.h"
#include "core/image.h"
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

  layout(location = 0)in vec2 uv;
  layout(location = 0) out vec4 color;
  layout (binding = 0) uniform sampler2D uTexture;

  void main(void)
  {
    color = texture(uTexture, uv);
  }
)";


bkk::core::render::texture_t createTexture(const bkk::core::render::context_t& context)
{
  bkk::core::render::texture_t texture;
  bkk::core::image::image2D_t image = {};
  if (!bkk::core::image::load("../resources/brokkr.png", false, &image))
  {
    fprintf(stderr,"Error loading texture\n");
  }
  else
  {
    //Create the texture
    bkk::core::render::texture2DCreate(context, &image, 1, bkk::core::render::texture_sampler_t(), &texture);
    bkk::core::image::unload(&image);
  }

  return texture;
}

void buildCommandBuffers(const bkk::core::render::context_t& context,const bkk::core::mesh::mesh_t& mesh,
                         bkk::core::render::descriptor_set_t* descriptorSet,
                         bkk::core::render::pipeline_layout_t* layout, bkk::core::render::graphics_pipeline_t* pipeline)
{
  const bkk::core::render::command_buffer_t* commandBuffers;
  uint32_t count = bkk::core::render::getPresentationCommandBuffers(context, &commandBuffers);
  for (uint32_t i(0); i<count; ++i)
  {
    bkk::core::render::beginPresentationCommandBuffer(context, i, nullptr);
    bkk::core::render::graphicsPipelineBind(commandBuffers[i], *pipeline);
    bkk::core::render::descriptorSetBind(commandBuffers[i], *layout, 0, descriptorSet, 1u);
    bkk::core::mesh::draw(commandBuffers[i], mesh);
    bkk::core::render::endPresentationCommandBuffer(context, i);
  }
}

int main()
{
  //Create a window
  bkk::core::window::window_t window;
  bkk::core::window::create("Textured Quad", 400u, 400u, &window);

  //Initialize context
  bkk::core::render::context_t context;
  bkk::core::render::contextCreate("Textured Quad", "", window, 3, &context);

  //Create a quad and a texture
  bkk::core::mesh::mesh_t mesh = bkk::core::mesh::fullScreenQuad(context);
  bkk::core::render::texture_t texture = createTexture(context);

  //Create descriptor pool
  bkk::core::render::descriptor_pool_t descriptorPool;
  bkk::core::render::descriptorPoolCreate( context, 1u,
                                bkk::core::render::combined_image_sampler_count(1u),
                                bkk::core::render::uniform_buffer_count(0u),
                                bkk::core::render::storage_buffer_count(0u),
                                bkk::core::render::storage_image_count(0u),
                                &descriptorPool);

  //Create descriptor layout
  bkk::core::render::descriptor_set_layout_t descriptorSetLayout;
  bkk::core::render::descriptor_binding_t binding = { bkk::core::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, bkk::core::render::descriptor_t::stage::FRAGMENT };
  bkk::core::render::descriptorSetLayoutCreate(context, &binding, 1u, &descriptorSetLayout);

  //Create pipeline layout
  bkk::core::render::pipeline_layout_t pipelineLayout;
  bkk::core::render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, nullptr, 0u, &pipelineLayout);

  //Create descriptor set
  bkk::core::render::descriptor_set_t descriptorSet;
  bkk::core::render::descriptor_t descriptor = bkk::core::render::getDescriptor(texture);
  bkk::core::render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &descriptor, &descriptorSet);

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
  buildCommandBuffers(context, mesh, &descriptorSet, &pipelineLayout, &pipeline);

  bool quit = false;
  while (!quit)
  {
    bkk::core::window::event_t* event = nullptr;
    while ((event = bkk::core::window::getNextEvent(&window)))
    {
      if (event->type_ == bkk::core::window::EVENT_QUIT)
      {
        quit = true;
      }
      else if (event->type_ == bkk::core::window::EVENT_RESIZE)
      {
        bkk::core::window::event_resize_t* resizeEvent = (bkk::core::window::event_resize_t*)event;
        swapchainResize(&context, resizeEvent->width_, resizeEvent->height_);
        buildCommandBuffers(context, mesh, &descriptorSet, &pipelineLayout, &pipeline);
      }
    }

    bkk::core::render::presentFrame(&context);
  }

  //Wait for all pending operations to be finished
  bkk::core::render::contextFlush(context);

  //Destroy all resources
  bkk::core::mesh::destroy(context, &mesh);
  bkk::core::render::textureDestroy(context, &texture);

  bkk::core::render::shaderDestroy(context, &vertexShader);
  bkk::core::render::shaderDestroy(context, &fragmentShader);

  bkk::core::render::graphicsPipelineDestroy(context, &pipeline);
  bkk::core::render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  bkk::core::render::descriptorSetDestroy(context, &descriptorSet);
  bkk::core::render::descriptorPoolDestroy(context, &descriptorPool);
  bkk::core::render::pipelineLayoutDestroy(context, &pipelineLayout);

  bkk::core::render::contextDestroy(&context);

  //Close window
  bkk::core::window::destroy(&window);

  return 0;
}
