/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef RENDER_H
#define RENDER_H

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include "core/render-types.h"

namespace bkk
{
  namespace core
  {
    namespace image { struct image2D_t; }
    namespace window { struct window_t; }

    namespace render
    {
      //Context
      void contextCreate(const char* applicationName, const char* engineName, const window::window_t& window, uint32_t swapChainImageCount, context_t* context);
      void contextDestroy(context_t* context);
      void contextFlush(const context_t& context);
      void swapchainResize(context_t* context, uint32_t width, uint32_t height);

      void beginPresentationCommandBuffer(const context_t& context, uint32_t index, VkClearValue* clearValues);
      uint32_t getPresentationCommandBuffers(const context_t& context, const command_buffer_t** commandBuffers);
      uint32_t getPresentationCommandBuffer(context_t& context, command_buffer_t** commandBuffer);
      void endPresentationCommandBuffer(const context_t& context, uint32_t index);

      void presentFrame(context_t* context, VkSemaphore* waitSemaphore = nullptr, uint32_t waitSemaphoreCount = 0u);

      //Shaders
      bool shaderCreateFromSPIRV(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader);
      bool shaderCreateFromGLSL(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader);
      bool shaderCreateFromGLSLSource(const context_t& context, shader_t::type_e type, const char* glslSource, shader_t* shader);
      void shaderDestroy(const context_t& context, shader_t* shader);

      //GPU memory
      gpu_memory_t gpuMemoryAllocate(const context_t& context, VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator = nullptr);
      void gpuMemoryDeallocate(const context_t& context, gpu_memory_allocator_t* allocator, gpu_memory_t memory);
      void* gpuMemoryMap(const context_t& context, gpu_memory_t memory);
      void* gpuMemoryMap(const context_t& context, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, gpu_memory_t memory);
      void gpuMemoryUnmap(const context_t& context, gpu_memory_t memory);
      void gpuAllocatorCreate(const context_t& context, size_t size, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator);
      void gpuAllocatorDestroy(const context_t& context, gpu_memory_allocator_t* allocator);

      //Textures
      void texture2DCreate(const context_t& context, const image::image2D_t* images, uint32_t mipLevels, texture_sampler_t sampler, texture_t* texture);
      void texture2DCreateAndGenerateMipmaps(const context_t& context, const image::image2D_t& image, texture_sampler_t sampler, texture_t* texture);
      void texture2DCreate(const context_t& context, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usageFlags, texture_sampler_t sampler, texture_t* texture);
      bool textureIsValid(const texture_t& texture);
      void textureDestroy(const context_t& context, texture_t* texture);

      void textureCopy(const command_buffer_t& commandBuffer, texture_t* srcTexture, texture_t* dstTexture,
        uint32_t width, uint32_t height, uint32_t dstMipmap = 0, uint32_t dstLayer = 0, uint32_t srcMipmap = 0, uint32_t srcLayer = 0);

      void textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subResourceRange, texture_t* texture);
      void textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, texture_t* texture);
      void textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout layout, texture_t* texture);

      void textureChangeLayoutNow(const context_t& context, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subResourceRange, texture_t* texture);
      void textureChangeLayoutNow(const context_t& context, VkImageLayout layout, texture_t* texture);

      void textureCubemapCreate(const context_t& context, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, texture_sampler_t sampler, texture_t* texture);
      void textureCubemapCreate(const context_t& context, const image::image2D_t* images, uint32_t mipLevels, texture_sampler_t sampler, texture_t* texture);
      void textureCubemapCreateFromEquirectangularImage(const context_t& context, const image::image2D_t& image, uint32_t size, bool generateMipmaps, texture_t* cubemap);


      //Buffers
      void gpuBufferCreate(const context_t& context, gpu_buffer_t::usage_e usage, uint32_t memoryType, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer);
      void gpuBufferCreate(const context_t& context, gpu_buffer_t::usage_e usage, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer);
      void gpuBufferDestroy(const context_t& context, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer);
      void gpuBufferUpdate(const context_t& context, void* data, size_t offset, size_t size, gpu_buffer_t* buffer);
      void* gpuBufferMap(const context_t& context, const gpu_buffer_t& buffer);
      void gpuBufferUnmap(const context_t& context, const gpu_buffer_t& buffer);

      //Descriptors
      void descriptorPoolCreate(const context_t& context, uint32_t descriptorSetsCount,
        combined_image_sampler_count combinedImageSamplers, uniform_buffer_count uniformBuffers,
        storage_buffer_count storageBuffers, storage_image_count storageImages,
        descriptor_pool_t* descriptorPool);

      void descriptorPoolDestroy(const context_t& context, descriptor_pool_t* descriptorPool);

      void descriptorSetCreate(const context_t& context, const descriptor_pool_t& descriptorPool, const descriptor_set_layout_t& descriptorSetLayout, descriptor_t* descriptors, descriptor_set_t* descriptorSet);
      void descriptorSetDestroy(const context_t& context, descriptor_set_t* descriptorSet);
      void descriptorSetUpdate(const context_t& context, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet);
      void descriptorSetBind(command_buffer_t commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount);
      void descriptorSetLayoutCreate(const context_t& context, descriptor_binding_t* bindings, uint32_t bindingCount, descriptor_set_layout_t* desriptorSetLayout);
      void descriptorSetLayoutDestroy(const context_t& context, descriptor_set_layout_t* desriptorSetLayout);

      descriptor_t getDescriptor(const gpu_buffer_t& buffer);
      descriptor_t getDescriptor(const texture_t& texture);

      //Pipelines
      void pipelineLayoutCreate(const context_t& context,
        descriptor_set_layout_t* descriptorSetLayouts, uint32_t descriptorSetLayoutCount,
        push_constant_range_t* pushConstantRanges, uint32_t pushConstantRangeCount,
        pipeline_layout_t* pipelineLayout);

      void pipelineLayoutDestroy(const context_t& context, pipeline_layout_t* pipelineLayout);

      void graphicsPipelineCreate(const context_t& context, VkRenderPass renderPass, uint32_t subpass,
        const render::vertex_format_t& vertexFormat,
        const pipeline_layout_t& pipelineLayout, const graphics_pipeline_t::description_t& pipelineDesc,
        graphics_pipeline_t* pipeline);

      void graphicsPipelineDestroy(const context_t& context, graphics_pipeline_t* pipeline);
      void graphicsPipelineBind(command_buffer_t commandBuffer, const graphics_pipeline_t& pipeline);

      void computePipelineCreate(const context_t& context, const pipeline_layout_t& pipelineLayout, const render::shader_t& computeShader, compute_pipeline_t* pipeline);
      void computePipelineDestroy(const context_t& context, compute_pipeline_t* pipeline);
      void computePipelineBind(command_buffer_t commandBuffer, const compute_pipeline_t& pipeline);
      void computeDispatch(command_buffer_t commandBuffer, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ);

      void pushConstants(command_buffer_t commandBuffer, pipeline_layout_t pipelineLayout, uint32_t offset, const void* constant);

      //Vertex formats
      void vertexFormatCreate(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format);
      void vertexFormatCopy(const vertex_format_t* formatSrc, vertex_format_t* formatDst);
      void vertexFormatAddAttributes(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format);
      void vertexFormatDestroy(vertex_format_t* format);

      //Command buffers
      //@TODO Allow the user to specify command buffer pool from which command buffers are allocated (Command buffers are allocated from global command buffer pool from the context)

      VkCommandPool commandPoolCreate(const context_t& context);
      void commandPoolDestroy(const context_t& context, VkCommandPool commandPool);

      void commandBufferCreate(const context_t& context, VkCommandBufferLevel level,
        VkSemaphore* waitSemaphore, VkPipelineStageFlags* waitStages, uint32_t waitSemaphoreCount,
        VkSemaphore* signalSemaphore, uint32_t signalSemaphoreCount, command_buffer_t::type_e type,
        VkCommandPool commandPool, command_buffer_t* commandBuffer);

      void commandBufferDestroy(const context_t& context, command_buffer_t* commandBuffer);
      void commandBufferBegin(const context_t& context, const command_buffer_t& commandBuffer);
      void commandBufferRenderPassBegin(const context_t& context, const frame_buffer_t* frameBuffer, VkClearValue* clearValues, uint32_t clearValuesCount, const command_buffer_t& commandBuffer);
      void commandBufferNextSubpass(const command_buffer_t& commandBuffer);

      void setViewport(const command_buffer_t& commandBuffer, int32_t x, int32_t y, uint32_t width, uint32_t height);
      void setScissor(const command_buffer_t& commandBuffer, int32_t x, int32_t y, uint32_t width, uint32_t height);

      void commandBufferRenderPassEnd(const command_buffer_t& commandBuffer);
      void commandBufferEnd(const command_buffer_t& commandBuffer);
      void commandBufferSubmit(const context_t& context, const command_buffer_t& commandBuffer);

      void commandBufferDebugMarkerBegin(const context_t& context, const command_buffer_t& commandBuffer, const char* markerName);
      void commandBufferDebugMarkerEnd(const context_t& context, const command_buffer_t& commandBuffer);

      VkSemaphore semaphoreCreate(const context_t& context);
      void semaphoreDestroy(const context_t& context, VkSemaphore semaphore);

      //Renderpass
      void renderPassCreate(const context_t& context,
        render_pass_t::attachment_t* attachments, uint32_t attachmentCount,
        render_pass_t::subpass_t* subpasses, uint32_t subpassCount,
        render_pass_t::subpass_dependency_t* dependencies, uint32_t dependencyCount,
        render_pass_t* renderPass);

      void renderPassDestroy(const context_t& context, render_pass_t* renderPass);

      //FrameBuffers
      void frameBufferCreate(const context_t& context, uint32_t width, uint32_t height, const render_pass_t& renderPass, VkImageView* imageViews, frame_buffer_t* frameBuffer);
      void frameBufferDestroy(const context_t& context, frame_buffer_t* frameBuffer);
      void depthStencilBufferCreate(const context_t& context, uint32_t width, uint32_t height, depth_stencil_buffer_t* depthStencilBuffer);
      void depthStencilBufferDestroy(const context_t& context, depth_stencil_buffer_t* depthStencilBuffer);

      //Utility functions    
      void diffuseConvolution(const context_t& context, texture_t environmentMap, uint32_t size, texture_t* irradiance);
      void specularConvolution(const context_t& context, texture_t environmentMap, uint32_t size, uint32_t maxMipmapLevels, texture_t* specularMap);
      void brdfConvolution(const context_t& context, uint32_t size, texture_t* brdfConvolution);
      void waitForAllCommandBuffersToFinish(const context_t& context);

    } //render
  }//core
}//bkk
#endif // RENDER_H