/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "framework/render-target.h"
#include "framework/renderer.h"

using namespace bkk;
using namespace bkk::core;
using namespace bkk::framework;

render_target_t::render_target_t()
:width_(0u),
 height_(0u),
 hasDepthBuffer_(false)
{
  target_ = {};
  depthStencilBuffer_ = {};
}

render_target_t::render_target_t(uint32_t width, uint32_t height,
  VkFormat format,
  bool depthBuffer,
  renderer_t* renderer)
  :width_(width),
  height_(height),
  format_(format),
  hasDepthBuffer_(depthBuffer)
{
  render::context_t& context = renderer->getContext();

  target_ = {};
  depthStencilBuffer_ = {};

  render::texture_sampler_t sampler = { render::texture_sampler_t::filter_mode_e::NEAREST,
    render::texture_sampler_t::filter_mode_e::NEAREST,
    render::texture_sampler_t::filter_mode_e::NEAREST,
    render::texture_sampler_t::wrap_mode_e::CLAMP_TO_EDGE,
    render::texture_sampler_t::wrap_mode_e::CLAMP_TO_EDGE};

  render::texture2DCreate(context, width, height, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, sampler, &target_);
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, &target_);

  if(depthBuffer)
   render::depthStencilBufferCreate(context, width, height, &depthStencilBuffer_);

}

void render_target_t::destroy(renderer_t* renderer)
{
  render::context_t& context = renderer->getContext();
  render::textureDestroy(context, &target_);

  if (hasDepthBuffer_)
    render::depthStencilBufferDestroy(context, &depthStencilBuffer_);
}