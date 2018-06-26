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

#include "gui.h"
#include "maths.h"
#include "image.h"

struct gui_context_t
{
  bkk::render::texture_t fontTexture_;
  bkk::render::descriptor_pool_t descriptorPool_;

  bkk::render::descriptor_set_layout_t descriptorSetLayout_;
  bkk::render::descriptor_set_t descriptorSet_;

  bkk::render::pipeline_layout_t pipelineLayout_;
  bkk::render::graphics_pipeline_t pipeline_;
  bkk::render::shader_t vertexShader_;
  bkk::render::shader_t fragmentShader_;
  bkk::render::vertex_format_t vertexFormat_;
  bkk::render::gpu_buffer_t vertexBuffer_ = {};
  bkk::render::gpu_buffer_t indexBuffer_ = {};
  bkk::maths::vec4 scaleAndOffset_;
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

static void createFontsTexture(const bkk::render::context_t& context)
{
  ImGuiIO& io = ImGui::GetIO();

  unsigned char* data;
  int width, height;
  io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);
  uint32_t size = width * height * 4 * sizeof(char);
  bkk::image::image2D_t image = { (uint32_t)width, (uint32_t)height, 4u, 1u, size, data };


  bkk::render::texture2DCreate(context, &image, 1u, bkk::render::texture_sampler_t(), &gGuiContext.fontTexture_);
  io.Fonts->TexID = (void *)(intptr_t)gGuiContext.fontTexture_.image_;
}

void bkk::gui::init(const bkk::render::context_t& context)
{
  if (!gGuiInitialized)
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    createFontsTexture(context);

    bkk::render::descriptorPoolCreate(context, 1u,
      bkk::render::combined_image_sampler_count(10u),
      bkk::render::uniform_buffer_count(10u),
      bkk::render::storage_buffer_count(10u),
      bkk::render::storage_image_count(10u),
      &gGuiContext.descriptorPool_);

    bkk::render::descriptor_binding_t binding = { bkk::render::descriptor_t::type::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
    bkk::render::descriptorSetLayoutCreate(context, &binding, 1u, &gGuiContext.descriptorSetLayout_);

    bkk::render::descriptor_t descriptor = bkk::render::getDescriptor(gGuiContext.fontTexture_);
    bkk::render::descriptorSetCreate(context, gGuiContext.descriptorPool_, gGuiContext.descriptorSetLayout_, &descriptor, &gGuiContext.descriptorSet_);

    bkk::render::push_constant_range_t pushConstantRanges = { VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 4, 0u };
    bkk::render::pipelineLayoutCreate(context, &gGuiContext.descriptorSetLayout_, 1u, &pushConstantRanges, 1u, &gGuiContext.pipelineLayout_);

    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::VERTEX_SHADER, vsSource, &gGuiContext.vertexShader_);
    bkk::render::shaderCreateFromGLSLSource(context, bkk::render::shader_t::FRAGMENT_SHADER, fsSource, &gGuiContext.fragmentShader_);

    //Create vertex format (vec2, vec2, vec4)
    uint32_t vertexSize = 2 * sizeof(bkk::maths::vec2) + sizeof(bkk::maths::vec4);
    bkk::render::vertex_attribute_t attributes[3] = {
    { bkk::render::vertex_attribute_t::format::VEC2, IM_OFFSETOF(ImDrawVert, pos), sizeof(ImDrawVert), false },
    { bkk::render::vertex_attribute_t::format::VEC2, IM_OFFSETOF(ImDrawVert, uv),  sizeof(ImDrawVert), false },
    { bkk::render::vertex_attribute_t::format::COLOR,IM_OFFSETOF(ImDrawVert, col), sizeof(ImDrawVert), false } };
    bkk::render::vertexFormatCreate(attributes, 3u, &gGuiContext.vertexFormat_);


    bkk::render::graphics_pipeline_t::description_t pipelineDesc = {};
    pipelineDesc.viewPort_ = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
    pipelineDesc.scissorRect_ = { { 0,0 },{ context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ } };
    pipelineDesc.blendState_.resize(1);
    pipelineDesc.blendState_[0].colorWriteMask = 0xF;
    pipelineDesc.blendState_[0].blendEnable = VK_TRUE;
    pipelineDesc.blendState_[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineDesc.blendState_[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState_[0].colorBlendOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendState_[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineDesc.blendState_[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineDesc.blendState_[0].alphaBlendOp = VK_BLEND_OP_ADD;
    pipelineDesc.blendState_[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipelineDesc.cullMode_ = VK_CULL_MODE_NONE;
    pipelineDesc.depthTestEnabled_ = false;
    pipelineDesc.depthWriteEnabled_ = false;
    pipelineDesc.vertexShader_ = gGuiContext.vertexShader_;
    pipelineDesc.fragmentShader_ = gGuiContext.fragmentShader_;
    bkk::render::graphicsPipelineCreate(context, context.swapChain_.renderPass_, 0u, gGuiContext.vertexFormat_, gGuiContext.pipelineLayout_, pipelineDesc, &gGuiContext.pipeline_);

    gGuiInitialized = true;
  }
}

void bkk::gui::destroy(const bkk::render::context_t& context)
{ 
  if (gGuiInitialized)
  {
    //Destroy resources
    bkk::render::textureDestroy(context, &gGuiContext.fontTexture_);    
    bkk::render::descriptorSetLayoutDestroy(context, &gGuiContext.descriptorSetLayout_);
    bkk::render::descriptorSetDestroy(context, &gGuiContext.descriptorSet_);
    bkk::render::pipelineLayoutDestroy(context, &gGuiContext.pipelineLayout_);
    bkk::render::graphicsPipelineDestroy(context, &gGuiContext.pipeline_);
    bkk::render::shaderDestroy(context, &gGuiContext.vertexShader_);
    bkk::render::shaderDestroy(context, &gGuiContext.fragmentShader_);
    bkk::render::vertexFormatDestroy(&gGuiContext.vertexFormat_);
    bkk::render::gpuBufferDestroy(context, nullptr, &gGuiContext.vertexBuffer_);
    bkk::render::gpuBufferDestroy(context, nullptr, &gGuiContext.indexBuffer_);
    bkk::render::descriptorPoolDestroy(context, &gGuiContext.descriptorPool_);
    
    ImGui::DestroyContext();
  }
};

void bkk::gui::beginFrame(const bkk::render::context_t& context)
{
  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(io.Fonts->IsBuilt());
  io.DisplaySize = ImVec2((float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_);
  io.DisplayFramebufferScale = ImVec2(1, 1);
  ImGui::NewFrame();
}

void bkk::gui::endFrame()
{
  ImGui::Render();
}


void bkk::gui::draw(const bkk::render::context_t& context, bkk::render::command_buffer_t commandBuffer)
{
  ImDrawData* draw_data = ImGui::GetDrawData();
  size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
  size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

  if (vertex_size == 0 || index_size == 0)
    return;

  if(gGuiContext.vertexBuffer_.memory_.size_ < vertex_size)
  {
    bkk::render::contextFlush(context);
    if (gGuiContext.vertexBuffer_.handle_ != VK_NULL_HANDLE)
      bkk::render::gpuBufferDestroy(context, nullptr, &gGuiContext.vertexBuffer_);

    bkk::render::gpuBufferCreate(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, nullptr, vertex_size, nullptr, &gGuiContext.vertexBuffer_);
  }

  if( gGuiContext.indexBuffer_.memory_.size_ < index_size)
  {
    bkk::render::contextFlush(context);
    if (gGuiContext.indexBuffer_.handle_ != VK_NULL_HANDLE)
      bkk::render::gpuBufferDestroy(context, nullptr, &gGuiContext.indexBuffer_);

    bkk::render::gpuBufferCreate(context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, nullptr, index_size, nullptr, &gGuiContext.indexBuffer_);
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
  range[0].memory = gGuiContext.vertexBuffer_.memory_.handle_;
  range[0].size = VK_WHOLE_SIZE;
  range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range[1].memory = gGuiContext.indexBuffer_.memory_.handle_;
  range[1].size = VK_WHOLE_SIZE;
  vkFlushMappedMemoryRanges(context.device_, 2u, range);
  render::gpuBufferUnmap(context, gGuiContext.vertexBuffer_);
  render::gpuBufferUnmap(context, gGuiContext.indexBuffer_);
  
  VkBuffer vertex_buffers[3] = { gGuiContext.vertexBuffer_.handle_,gGuiContext.vertexBuffer_.handle_,gGuiContext.vertexBuffer_.handle_ };
  VkDeviceSize vertex_offsets[3] = {};
  vkCmdBindVertexBuffers(commandBuffer.handle_, 0, 3, vertex_buffers, vertex_offsets);
  vkCmdBindIndexBuffer(commandBuffer.handle_, gGuiContext.indexBuffer_.handle_, 0, VK_INDEX_TYPE_UINT16);

  gGuiContext.scaleAndOffset_.x = 2.0f / draw_data->DisplaySize.x;
  gGuiContext.scaleAndOffset_.y = 2.0f / draw_data->DisplaySize.y;
  gGuiContext.scaleAndOffset_.z = -1.0f - draw_data->DisplayPos.x * gGuiContext.scaleAndOffset_.x;
  gGuiContext.scaleAndOffset_.w = -1.0f - draw_data->DisplayPos.y * gGuiContext.scaleAndOffset_.y;

  bkk::render::graphicsPipelineBind(commandBuffer, gGuiContext.pipeline_);
  bkk::render::descriptorSetBindForGraphics(commandBuffer, gGuiContext.pipelineLayout_, 0, &gGuiContext.descriptorSet_, 1u);
  bkk::render::pushConstants(commandBuffer, gGuiContext.pipelineLayout_, 0u, &gGuiContext.scaleAndOffset_);

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
        vkCmdSetScissor(commandBuffer.handle_, 0, 1, &scissor);

        // Draw
        vkCmdDrawIndexed(commandBuffer.handle_, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
      }
      indexOffset += pcmd->ElemCount;
    }
    vertexOffset += cmd_list->VtxBuffer.Size;
  }
}

void bkk::gui::updateMousePosition(float x, float y)
{
  ImGuiIO& io = ImGui::GetIO();
  io.MousePos = ImVec2(x, y);
}

void bkk::gui::updateMouseButton(uint32_t button, bool pressed)
{
  ImGuiIO& io = ImGui::GetIO();
  io.MouseDown[button] = pressed;
}