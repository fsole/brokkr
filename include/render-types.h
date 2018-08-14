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

#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

#include <vulkan/vulkan.h>
#include "dynamic-array.h"

namespace bkk
{
  namespace render
  {
    enum gpu_memory_type_e{
      HOST_VISIBLE = 1,
      DEVICE_LOCAL = 2,
      HOST_COHERENT = 4,
      HOST_VISIBLE_COHERENT = HOST_VISIBLE | HOST_COHERENT
    };

    struct gpu_memory_t
    {
      VkDeviceMemory handle_;
      VkDeviceSize offset_;
      VkDeviceSize size_;
    };

    struct gpu_memory_allocator_t
    {
      VkDeviceMemory memory_;
      VkDeviceSize size_;
      VkDeviceSize head_;
    };

    struct queue_t
    {
      VkQueue handle_;
      int32_t queueIndex_;
    };

    struct depth_stencil_buffer_t
    {
      VkFormat format_;
      VkImageLayout layout_;
      VkImageAspectFlags aspectFlags_;
      VkImage image_;
      gpu_memory_t memory_;
      VkImageView imageView_;
      VkDescriptorImageInfo descriptor_;
    };

    struct surface_t
    {
      VkSurfaceKHR handle_;
      VkFormat imageFormat_;
      VkColorSpaceKHR colorSpace_;
      VkSurfaceTransformFlagBitsKHR preTransform_;
    };

    struct command_buffer_t
    {
      enum type{
        GRAPHICS = 0,
        COMPUTE = 1
      };

      VkCommandBuffer handle_ = VK_NULL_HANDLE;
      type type_;

      uint32_t waitSemaphoreCount_;
      VkSemaphore* waitSemaphore_;
      VkPipelineStageFlags* waitStages_;

      uint32_t signalSemaphoreCount_;
      VkSemaphore* signalSemaphore_;
      VkFence fence_;
    };

    struct swapchain_t
    {
      VkSwapchainKHR handle_;

      uint32_t imageCount_;
      uint32_t currentImage_;
      uint32_t imageWidth_;
      uint32_t imageHeight_;

      dynamic_array_t<VkImage> image_;
      dynamic_array_t<VkImageView> imageView_;
      depth_stencil_buffer_t depthStencil_;

      dynamic_array_t<VkFramebuffer> frameBuffer_;
      dynamic_array_t<command_buffer_t> commandBuffer_;
      
      VkRenderPass renderPass_;

      VkSemaphore imageAcquired_;
      VkSemaphore renderingComplete_;
    };

    struct context_t
    {
      VkInstance instance_;
      VkPhysicalDevice physicalDevice_;
      VkDevice device_;
      VkPhysicalDeviceMemoryProperties memoryProperties_;
      VkCommandPool commandPool_;
      queue_t graphicsQueue_;
      queue_t computeQueue_;
      surface_t surface_;
      swapchain_t swapChain_;
      VkDebugReportCallbackEXT debugCallback_;

      //Imported functions
      PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
      PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
      PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
      PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
      PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
      PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
      PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
      PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
      PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
      PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
      PFN_vkQueuePresentKHR vkQueuePresentKHR;
    };

    struct texture_t
    {
      VkImage image_;
      gpu_memory_t memory_;
      VkImageView imageView_;
      VkSampler sampler_;
      VkImageLayout layout_;
      VkFormat format_;
      VkImageAspectFlags aspectFlags_;
      uint32_t mipLevels_;
      VkExtent3D extent_;
      VkDescriptorImageInfo descriptor_;
    };
    
    struct texture_sampler_t
    {
      enum struct filter_mode{
        NEAREST = 0,
        LINEAR = 1
      };

      enum struct wrap_mode{
        REPEAT = 0,
        MIRRORED_REPEAT = 1,
        CLAMP_TO_EDGE = 2,
        CLAMP_TO_BORDER = 3,
        MIRROR_CLAMP_TO_EDGE = 4
      };

      filter_mode minification_ = filter_mode::LINEAR;
      filter_mode magnification_ = filter_mode::LINEAR;
      filter_mode mipmap_ = filter_mode::LINEAR;
      wrap_mode wrapU_ = wrap_mode::MIRRORED_REPEAT;
      wrap_mode wrapV_ = wrap_mode::MIRRORED_REPEAT;
      wrap_mode wrapW_ = wrap_mode::MIRRORED_REPEAT;
    };

    struct gpu_buffer_t
    {
      enum usage{
        TRANSFER_SRC = 0x00000001,
        TRANSFER_DST = 0x00000002,
        UNIFORM_TEXEL_BUFFER = 0x00000004,
        STORAGE_TEXEL_BUFFER = 0x00000008,
        UNIFORM_BUFFER = 0x00000010,
        STORAGE_BUFFER = 0x00000020,
        INDEX_BUFFER = 0x00000040,
        VERTEX_BUFFER = 0x00000080,
        INDIRECT_BUFFER = 0x00000100,
      };

      VkBuffer handle_;
      gpu_memory_t memory_ = {};
      uint32_t usage_;
      VkDescriptorBufferInfo descriptor_;
    };

    struct descriptor_t
    {
      enum struct type
      {
        SAMPLER = 0,
        COMBINED_IMAGE_SAMPLER = 1,
        SAMPLED_IMAGE = 2,
        STORAGE_IMAGE = 3,
        UNIFORM_TEXEL_BUFFER = 4,
        STORAGE_TEXEL_BUFFER = 5,
        UNIFORM_BUFFER = 6,
        STORAGE_BUFFER = 7,
        UNIFORM_BUFFER_DYNAMIC = 8,
        STORAGE_BUFFER_DYNAMIC = 9,
        INPUT_ATTACHMENT = 10
      };

      enum stage
      {
        VERTEX = 0x00000001,
        TESSELLATION_CONTROL = 0x00000002,
        TESSELLATION_EVALUATION = 0x00000004,
        GEOMETRY = 0x00000008,
        FRAGMENT = 0x00000010,
        COMPUTE = 0x00000020
      };

      VkDescriptorBufferInfo bufferDescriptor_;
      VkDescriptorImageInfo imageDescriptor_;
    };

    struct descriptor_binding_t
    {
      descriptor_t::type type_;
      uint32_t binding_;
      uint32_t stageFlags_;
    };

    struct descriptor_set_layout_t
    {
      VkDescriptorSetLayout handle_;
      uint32_t bindingCount_;
      descriptor_binding_t* bindings_;
    };

    struct push_constant_range_t
    {
      VkShaderStageFlags stageFlags_;
      uint32_t size_;
      uint32_t offset_;
    };

    struct pipeline_layout_t
    {
      VkPipelineLayout handle_;
      uint32_t descriptorSetLayoutCount_;
      descriptor_set_layout_t* descriptorSetLayout_;

      uint32_t pushConstantRangeCount_;
      push_constant_range_t* pushConstantRange_;
    };

    struct descriptor_pool_t
    {
      VkDescriptorPool handle_;
      uint32_t descriptorSets_;
      uint32_t combinedImageSamplers_;
      uint32_t uniformBuffers_;
      uint32_t storageBuffers_;
      uint32_t storageImages_;
    };

    struct descriptor_set_t
    {
      VkDescriptorSet handle_;
      uint32_t descriptorCount_;
      descriptor_t* descriptors_;
      descriptor_pool_t pool_;
    };

    struct shader_t
    {
      enum type{
        VERTEX_SHADER,
        FRAGMENT_SHADER,
        TESSELLATION_SHADER,
        COMPUTE_SHADER
      };

      VkShaderModule handle_;
      type type_;
    };

    struct graphics_pipeline_t
    {

      VkPipeline handle_;

      struct description_t
      {
        VkViewport viewPort_;
        VkRect2D scissorRect_;
        dynamic_array_t<VkPipelineColorBlendAttachmentState> blendState_;
        VkCullModeFlags cullMode_;
        bool depthTestEnabled_;
        bool depthWriteEnabled_;
        VkCompareOp depthTestFunction_;
        shader_t vertexShader_;
        shader_t fragmentShader_;
        //@TODO Stencil
        //@TODO Multisampling
      };

      description_t desc_;
    };

    struct compute_pipeline_t
    {
      VkPipeline handle_;
      shader_t computeShader_;
    };


    struct vertex_attribute_t
    {
      enum format{
        INT = 0,
        UINT = 1,
        FLOAT = 2,
        SVEC2 = 3,
        UVEC2 = 4,
        VEC2 = 5,
        SVEC3 = 6,
        UVEC3 = 7,
        VEC3 = 8,
        SVEC4 = 9,
        UVEC4 = 10,
        VEC4 = 11,
        COLOR = 12,
        ATTRIBUTE_FORMAT_COUNT
      };

      format format_;
      uint32_t offset_;
      uint32_t stride_;
      bool instanced_;
    };

    struct vertex_format_t
    {
      VkPipelineVertexInputStateCreateInfo vertexInputState_;
      VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_;

      vertex_attribute_t* attributes_;
      uint32_t attributeCount_;

      uint32_t vertexSize_;
    };

    struct render_pass_t
    {
      struct attachment_t
      {
        VkFormat format_;
        VkSampleCountFlagBits samples_;
        VkImageLayout initialLayout_;
        VkImageLayout finallLayout_;
        VkAttachmentStoreOp storeOp_;
        VkAttachmentLoadOp loadOp_;
      };


      struct subpass_dependency_t
      {
        uint32_t                srcSubpass;
        uint32_t                dstSubpass;
        VkPipelineStageFlags    srcStageMask;
        VkPipelineStageFlags    dstStageMask;
        VkAccessFlags           srcAccessMask;
        VkAccessFlags           dstAccessMask;
      };

      struct subpass_t
      {
        dynamic_array_t<uint32_t> colorAttachmentIndex_;
        dynamic_array_t<uint32_t> inputAttachmentIndex_;
        int32_t depthStencilAttachmentIndex_ = -1;
      };
      
      VkRenderPass handle_;

      uint32_t attachmentCount_;
      attachment_t* attachment_;
    };

    struct frame_buffer_t
    {
      VkFramebuffer handle_;
      uint32_t width_;
      uint32_t height_;

      render_pass_t renderPass_;
    };

    struct combined_image_sampler_count { combined_image_sampler_count(uint32_t count) :data_(count) {} uint32_t data_; };
    struct uniform_buffer_count { uniform_buffer_count(uint32_t count) :data_(count) {} uint32_t data_; };
    struct storage_buffer_count { storage_buffer_count(uint32_t count) :data_(count) {} uint32_t data_; };
    struct storage_image_count { storage_image_count(uint32_t count) :data_(count) {} uint32_t data_; };
  }
}

#endif