
#include "framework/frame-buffer.h"
#include "framework/renderer.h"


using namespace bkk;
using namespace bkk::core;
using namespace bkk::framework;

frame_buffer_t::frame_buffer_t()
:renderPass_(),
 frameBuffer_(),
 width_(0),
 height_(0),
 targetCount_(0),
 renderTargets_(nullptr) 
{  
}

frame_buffer_t::frame_buffer_t(render_target_handle_t* renderTargets, uint32_t targetCount,
  VkImageLayout* initialLayouts, VkImageLayout* finalLayouts, renderer_t* renderer)
{
  targetCount_ = targetCount;
  renderTargets_ = new render_target_handle_t[targetCount];
  memcpy(renderTargets_, renderTargets, sizeof(render_target_handle_t)*targetCount);

  std::vector<render::render_pass_t::attachment_t> attachments(targetCount);
  std::vector<VkImageView> imageViews(targetCount);  
  render::render_pass_t::subpass_t subpass;
  width_ = height_ = 0;
  for (uint32_t i = 0; i < targetCount; ++i)
  {
    render_target_t* target = renderer->getRenderTarget(renderTargets[i]);
    width_ = width_ == 0 ? target->getWidth() : width_;
    height_ = height_ == 0 ? target->getHeight() : height_;

    if (width_ != target->getWidth() || height_ != target->getHeight())
    {
      //error!
    }

    attachments[i].format_ = target->getFormat();
    attachments[i].initialLayout_ = initialLayouts != nullptr ? initialLayouts[i] : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[i].finallLayout_ = finalLayouts != nullptr ? finalLayouts[i] : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[i].storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[i].loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[i].samples_ = VK_SAMPLE_COUNT_1_BIT;
    subpass.colorAttachmentIndex_.push_back(i);
    imageViews[i] = target->getColorBuffer().imageView_;
    if (target->hasDepthBuffer())
    {
      core::render::depth_stencil_buffer_t depthBuffer = target->getDepthBuffer();
      render::render_pass_t::attachment_t depthAttachment;
      depthAttachment.format_ = depthBuffer.format_;
      depthAttachment.initialLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthAttachment.finallLayout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthAttachment.storeOp_ = VK_ATTACHMENT_STORE_OP_STORE;
      depthAttachment.loadOp_ = VK_ATTACHMENT_LOAD_OP_CLEAR;
      depthAttachment.samples_ = VK_SAMPLE_COUNT_1_BIT;
      attachments.push_back(depthAttachment);

      subpass.depthStencilAttachmentIndex_ = targetCount;
      imageViews.push_back(depthBuffer.imageView_);
    }
  }
  
  renderPass_ = {};
  frameBuffer_ = {};

  render::context_t& context = renderer->getContext();
  render::renderPassCreate(context, attachments.data(), (uint32_t)attachments.size(), &subpass, 1u, nullptr, 0u, &renderPass_);  
  render::frameBufferCreate(context, width_, height_, renderPass_, imageViews.data(), &frameBuffer_);
}

void frame_buffer_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();

  if (renderPass_.handle_ != VK_NULL_HANDLE)
    render::renderPassDestroy(context, &renderPass_);

  if (frameBuffer_.handle_ != VK_NULL_HANDLE)
    render::frameBufferDestroy(context, &frameBuffer_);

  if (renderTargets_ != nullptr)
    delete[] renderTargets_;
}