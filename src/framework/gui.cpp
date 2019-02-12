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

#include "framework/gui.h"

#include "core/maths.h"
#include "core/image.h"

using namespace bkk::core;
using namespace bkk::framework;

struct gui_context_t
{
  render::texture_t fontTexture_;
  render::descriptor_pool_t descriptorPool_;

  render::descriptor_set_layout_t descriptorSetLayout_;
  render::descriptor_set_t descriptorSet_;

  render::pipeline_layout_t pipelineLayout_;
  render::graphics_pipeline_t pipeline_;
  render::shader_t vertexShader_;
  render::shader_t fragmentShader_;
  render::vertex_format_t vertexFormat_;
  render::gpu_buffer_t vertexBuffer_ = {};
  render::gpu_buffer_t indexBuffer_ = {};
  maths::vec4 scaleAndOffset_;
};

static gui_context_t gGuiContext = {};
static bool gGuiInitialized = false;

static const char* vsSource = R"(
  #version 450 core
  layout(location = 0) in vec2 aPos;
  layout(location = 1) in vec2 aUV;
  layout(location = 2) in vec4 aColor;

  layout(push_constant) uniform uPushConstant
  {
    vec2 uScale;
    vec2 uTranslate;
  }pc;

  layout(location = 0) out struct
  {
    vec4 Color;
    vec2 UV;
  }Out;

  void main()
  {
    Out.Color = aColor;
    Out.UV = aUV;
    gl_Position = vec4(aPos*pc.uScale+pc.uTranslate, 0, 1);
  }
)";

static const char* fsSource = R"(
  #version 450 core
  layout(location = 0) out vec4 fColor;
  layout(set=0, binding=0) uniform sampler2D sTexture;
  layout(location = 0) in struct
  {
    vec4 Color;
    vec2 UV;
  }In;

  void main()
  {
    fColor = In.Color * texture(sTexture, In.UV.st);
  }

)";

static void createFontsTexture(const render::context_t& context)
{
  ImGuiIO& io = ImGui::GetIO();

  unsigned char* data;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);
  uint32_t size = width * height * 4 * sizeof(char);
  image::image2D_t image = { (uint32_t)width, (uint32_t)height, 4u, 1u, size, data };


  render::texture2DCreate(context, &image, 1u, render::texture_sampler_t(), &gGuiContext.fontTexture_);
  io.Fonts->TexID = (void *)(intptr_t)gGuiContext.fontTexture_.image;
}

void gui::init(const render::context_t& context)
{
  if (!gGuiInitialized)
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    createFontsTexture(context);

      render::descriptorPoolCreate(context, 1u,
      render::combined_image_sampler_count(10u),
      render::uniform_buffer_count(10u),
      render::storage_buffer_count(10u),
      render::storage_image_count(10u),
      &gGuiContext.descriptorPool_);

    render::descriptor_binding_t binding = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
    render::descriptorSetLayoutCreate(context, &binding, 1u, &gGuiContext.descriptorSetLayout_);

    render::descriptor_t descriptor = render::getDescriptor(gGuiContext.fontTexture_);
    render::descriptorSetCreate(context, gGuiContext.descriptorPool_, gGuiContext.descriptorSetLayout_, &descriptor, &gGuiContext.descriptorSet_);

    render::push_constant_range_t pushConstantRanges = { VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 4, 0u };
    render::pipelineLayoutCreate(context, &gGuiContext.descriptorSetLayout_, 1u, &pushConstantRanges, 1u, &gGuiContext.pipelineLayout_);

    render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &gGuiContext.vertexShader_);
    render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &gGuiContext.fragmentShader_);

    //Create vertex format (vec2, vec2, vec4)
    uint32_t vertexSize = 2 * sizeof(maths::vec2) + sizeof(maths::vec4);
    render::vertex_attribute_t attributes[3] = {
      { render::vertex_attribute_t::format_e::VEC2, IM_OFFSETOF(ImDrawVert, pos), sizeof(ImDrawVert), false },
      { render::vertex_attribute_t::format_e::VEC2, IM_OFFSETOF(ImDrawVert, uv),  sizeof(ImDrawVert), false },
      { render::vertex_attribute_t::format_e::COLOR,IM_OFFSETOF(ImDrawVert, col), sizeof(ImDrawVert), false } };
    render::vertexFormatCreate(attributes, 3u, &gGuiContext.vertexFormat_);


    render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
    pipelineDesc.scissorRect = { { 0,0 },{ context.swapChain.imageWidth,context.swapChain.imageHeight } };
    pipelineDesc.blendState.resize(1);
    pipelineDesc.blendState[0].colorWriteMask = 0xF;
    pipelineDesc.blendState[0].blendEnable = VK_TRUE;
    pipelineDesc.blendState[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineDesc.blendState[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState[0].colorBlendOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendState[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineDesc.blendState[0].alphaBlendOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipelineDesc.cullMode = VK_CULL_MODE_NONE;
    pipelineDesc.depthTestEnabled = false;
    pipelineDesc.depthWriteEnabled = false;
    pipelineDesc.vertexShader = gGuiContext.vertexShader_;
    pipelineDesc.fragmentShader = gGuiContext.fragmentShader_;
    render::graphicsPipelineCreate(context, context.swapChain.renderPass, 0u, gGuiContext.vertexFormat_, gGuiContext.pipelineLayout_, pipelineDesc, &gGuiContext.pipeline_);

    gGuiInitialized = true;
  }
}

void gui::destroy(const render::context_t& context)
{ 
  if (gGuiInitialized)
  {
    //Destroy resources
    render::textureDestroy(context, &gGuiContext.fontTexture_);    
    render::descriptorSetLayoutDestroy(context, &gGuiContext.descriptorSetLayout_);
    render::descriptorSetDestroy(context, &gGuiContext.descriptorSet_);
    render::pipelineLayoutDestroy(context, &gGuiContext.pipelineLayout_);
    render::graphicsPipelineDestroy(context, &gGuiContext.pipeline_);
    render::shaderDestroy(context, &gGuiContext.vertexShader_);
    render::shaderDestroy(context, &gGuiContext.fragmentShader_);
    render::vertexFormatDestroy(&gGuiContext.vertexFormat_);
    render::gpuBufferDestroy(context, nullptr, &gGuiContext.vertexBuffer_);
    render::gpuBufferDestroy(context, nullptr, &gGuiContext.indexBuffer_);
    render::descriptorPoolDestroy(context, &gGuiContext.descriptorPool_);
    
    ImGui::DestroyContext();
  }
};

void gui::beginFrame(const render::context_t& context)
{
  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(io.Fonts->IsBuilt());
  io.DisplaySize = ImVec2((float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight);
  io.DisplayFramebufferScale = ImVec2(1, 1);
  ImGui::NewFrame();
}

void gui::endFrame()
{
  ImGui::Render();
}


void gui::draw(const render::context_t& context, render::command_buffer_t commandBuffer)
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertex_size == 0 || index_size == 0)
    return;

  if(gGuiContext.vertexBuffer_.memory.size < vertex_size)
  {
    render::contextFlush(context);
    if (gGuiContext.vertexBuffer_.handle != VK_NULL_HANDLE)
      render::gpuBufferDestroy(context, nullptr, &gGuiContext.vertexBuffer_);

    render::gpuBufferCreate(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr, vertex_size, nullptr, &gGuiContext.vertexBuffer_);
  }

  if( gGuiContext.indexBuffer_.memory.size < index_size)
  {
    render::contextFlush(context);
    if (gGuiContext.indexBuffer_.handle != VK_NULL_HANDLE)
      render::gpuBufferDestroy(context, nullptr, &gGuiContext.indexBuffer_);

    render::gpuBufferCreate(context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nullptr, index_size, nullptr, &gGuiContext.indexBuffer_);
  }

  ImDrawVert* vertexData = (ImDrawVert*)render::gpuBufferMap(context, gGuiContext.vertexBuffer_);
  ImDrawIdx* indexData = (ImDrawIdx*)render::gpuBufferMap(context, gGuiContext.indexBuffer_);
  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];
    memcpy(vertexData, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
    memcpy(indexData, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
    vertexData += cmd_list->VtxBuffer.Size;
    indexData += cmd_list->IdxBuffer.Size;
  }

  //Flush buffers
  VkMappedMemoryRange range[2] = {};
  range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range[0].memory = gGuiContext.vertexBuffer_.memory.handle;
  range[0].size = VK_WHOLE_SIZE;
  range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range[1].memory = gGuiContext.indexBuffer_.memory.handle;
  range[1].size = VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(context.device, 2u, range);
  render::gpuBufferUnmap(context, gGuiContext.vertexBuffer_);
  render::gpuBufferUnmap(context, gGuiContext.indexBuffer_);
  
  VkBuffer vertex_buffers[3] = { gGuiContext.vertexBuffer_.handle,gGuiContext.vertexBuffer_.handle,gGuiContext.vertexBuffer_.handle };
  VkDeviceSize vertex_offsets[3] = {};
  vkCmdBindVertexBuffers(commandBuffer.handle, 0, 3, vertex_buffers, vertex_offsets);
  vkCmdBindIndexBuffer(commandBuffer.handle, gGuiContext.indexBuffer_.handle, 0, VK_INDEX_TYPE_UINT16);

  gGuiContext.scaleAndOffset_.x = 2.0f / draw_data->DisplaySize.x;
  gGuiContext.scaleAndOffset_.y = 2.0f / draw_data->DisplaySize.y;
  gGuiContext.scaleAndOffset_.z = -1.0f - draw_data->DisplayPos.x * gGuiContext.scaleAndOffset_.x;
  gGuiContext.scaleAndOffset_.w = -1.0f - draw_data->DisplayPos.y * gGuiContext.scaleAndOffset_.y;

  render::graphicsPipelineBind(commandBuffer, gGuiContext.pipeline_);
  render::descriptorSetBind(commandBuffer, gGuiContext.pipelineLayout_, 0, &gGuiContext.descriptorSet_, 1u);
  render::pushConstants(commandBuffer, gGuiContext.pipelineLayout_, 0u, &gGuiContext.scaleAndOffset_);

  int vertexOffset = 0;
  int indexOffset = 0;
  ImVec2 display_pos = draw_data->DisplayPos;
  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];
    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
    {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback)
      {
        pcmd->UserCallback(cmd_list, pcmd);
      }
      else
      {
        // Apply scissor/clipping rectangle
        // FIXME: We could clamp width/height based on clamped min/max values.
        VkRect2D scissor;
        scissor.offset.x = (int32_t)(pcmd->ClipRect.x - display_pos.x) > 0 ? (int32_t)(pcmd->ClipRect.x - display_pos.x) : 0;
        scissor.offset.y = (int32_t)(pcmd->ClipRect.y - display_pos.y) > 0 ? (int32_t)(pcmd->ClipRect.y - display_pos.y) : 0;
        scissor.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
        scissor.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y + 1); // FIXME: Why +1 here?
        vkCmdSetScissor(commandBuffer.handle, 0, 1, &scissor);

        // Draw
        vkCmdDrawIndexed(commandBuffer.handle, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
      }
      indexOffset += pcmd->ElemCount;
    }
    vertexOffset += cmd_list->VtxBuffer.Size;
  }
}

void gui::updateMousePosition(float x, float y)
{
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(x, y);
}

void gui::updateMouseButton(uint32_t button, bool pressed)
{
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDown[button] = pressed;
}