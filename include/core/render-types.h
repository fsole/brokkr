/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef RENDER_TYPES_H
#define RENDER_TYPES_H

#include <vulkan/vulkan.h>
#include "vector"

namespace bkk
{
  namespace core
  {
    namespace render
    {
      enum gpu_memory_type_e {
        HOST_VISIBLE = 1,
        DEVICE_LOCAL = 2,
        HOST_COHERENT = 4,
        HOST_VISIBLE_COHERENT = HOST_VISIBLE | HOST_COHERENT
      };

      struct gpu_memory_t
      {
        VkDeviceMemory handle;
        VkDeviceSize offset;
        VkDeviceSize size;
      };

      struct gpu_memory_allocator_t
      {
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkDeviceSize head;
      };

      struct queue_t
      {
        VkQueue handle;
        int32_t queueIndex;
      };

      struct depth_stencil_buffer_t
      {
        VkFormat format;
        VkImageLayout layout;
        VkImageAspectFlags aspectFlags;
        VkImage image;
        gpu_memory_t memory;
        VkImageView imageView;
        VkDescriptorImageInfo descriptor;
      };

      struct surface_t
      {
        VkSurfaceKHR handle;
        VkFormat imageFormat;
        VkColorSpaceKHR colorSpace;
        VkSurfaceTransformFlagBitsKHR preTransform;
      };

      struct command_buffer_t
      {
        enum type_e {
          GRAPHICS = 0,
          COMPUTE = 1
        };

        VkCommandBuffer handle = VK_NULL_HANDLE;
        type_e type;

        uint32_t waitSemaphoreCount;
        VkSemaphore* waitSemaphore;
        VkPipelineStageFlags* waitStages;

        uint32_t signalSemaphoreCount;
        VkSemaphore* signalSemaphore;
        VkFence fence;
      };

      struct swapchain_t
      {
        VkSwapchainKHR handle;

        uint32_t imageCount;
        uint32_t currentImage;
        uint32_t imageWidth;
        uint32_t imageHeight;

        std::vector<VkImage> image;
        std::vector<VkImageView> imageView;
        depth_stencil_buffer_t depthStencil;

        std::vector<VkFramebuffer> frameBuffer;
        std::vector<command_buffer_t> commandBuffer;

        VkRenderPass renderPass;

        VkSemaphore imageAcquired;
        VkSemaphore renderingComplete;
      };

      struct context_t
      {
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkPhysicalDeviceMemoryProperties memoryProperties;
        VkCommandPool commandPool;
        queue_t graphicsQueue;
        queue_t computeQueue;
        surface_t surface;
        swapchain_t swapChain;
        VkDebugReportCallbackEXT debugCallback;

        //Imported functions
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
        PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = nullptr;
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;

        PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
        PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
        PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
        PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
        PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
        PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT = nullptr;
        PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT = nullptr;
      };

      struct texture_t
      {
        VkImage image;
        gpu_memory_t memory;
        VkImageView imageView;
        VkSampler sampler;
        VkImageLayout layout;
        VkFormat format;
        VkImageAspectFlags aspectFlags;
        uint32_t mipLevels;
        VkExtent3D extent;
        VkDescriptorImageInfo descriptor;
      };

      struct texture_sampler_t
      {
        enum struct filter_mode_e {
          NEAREST = 0,
          LINEAR = 1
        };

        enum struct wrap_mode_e {
          REPEAT = 0,
          MIRRORED_REPEAT = 1,
          CLAMP_TO_EDGE = 2,
          CLAMP_TO_BORDER = 3,
          MIRROR_CLAMP_TO_EDGE = 4
        };

        filter_mode_e minification = filter_mode_e::LINEAR;
        filter_mode_e magnification = filter_mode_e::LINEAR;
        filter_mode_e mipmap = filter_mode_e::LINEAR;
        wrap_mode_e wrapU = wrap_mode_e::MIRRORED_REPEAT;
        wrap_mode_e wrapV = wrap_mode_e::MIRRORED_REPEAT;
        wrap_mode_e wrapW = wrap_mode_e::MIRRORED_REPEAT;
      };

      struct gpu_buffer_t
      {
        enum usage_e {
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

        VkBuffer handle;
        gpu_memory_t memory = {};
        uint32_t usage;
        VkDescriptorBufferInfo descriptor;
      };

      struct descriptor_t
      {
        enum struct type_e
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

        enum stage_e
        {
          VERTEX = 0x00000001,
          TESSELLATION_CONTROL = 0x00000002,
          TESSELLATION_EVALUATION = 0x00000004,
          GEOMETRY = 0x00000008,
          FRAGMENT = 0x00000010,
          COMPUTE = 0x00000020
        };

        VkDescriptorBufferInfo bufferDescriptor;
        VkDescriptorImageInfo imageDescriptor;
      };

      struct descriptor_binding_t
      {
        descriptor_t::type_e type;
        uint32_t binding;
        uint32_t stageFlags;
      };

      struct descriptor_set_layout_t
      {
        VkDescriptorSetLayout handle;
        uint32_t bindingCount;
        descriptor_binding_t* bindings;
      };

      struct push_constant_range_t
      {
        VkShaderStageFlags stageFlags;
        uint32_t size;
        uint32_t offset;
      };

      struct pipeline_layout_t
      {
        VkPipelineLayout handle;
        uint32_t descriptorSetLayoutCount;
        descriptor_set_layout_t* descriptorSetLayout;

        uint32_t pushConstantRangeCount;
        push_constant_range_t* pushConstantRange;
      };

      struct descriptor_pool_t
      {
        VkDescriptorPool handle;
        uint32_t descriptorSets;
        uint32_t combinedImageSamplers;
        uint32_t uniformBuffers;
        uint32_t storageBuffers;
        uint32_t storageImages;
      };

      struct descriptor_set_t
      {
        VkDescriptorSet handle;
        uint32_t descriptorCount;
        descriptor_t* descriptors;
        descriptor_pool_t pool;
      };

      struct shader_t
      {
        enum type_e {
          VERTEX_SHADER,
          FRAGMENT_SHADER,
          TESSELLATION_SHADER,
          COMPUTE_SHADER
        };

        VkShaderModule handle;
        type_e type;
      };

      struct pipeline_t
      {
        VkPipeline handle;
      };

      struct graphics_pipeline_t : pipeline_t
      {
        struct description_t
        {
          VkViewport viewPort;
          VkRect2D scissorRect;
          std::vector<VkPipelineColorBlendAttachmentState> blendState;
          VkCullModeFlags cullMode;
          bool depthTestEnabled;
          bool depthWriteEnabled;
          VkCompareOp depthTestFunction;
          shader_t vertexShader;
          shader_t fragmentShader;
          //@TODO Stencil
          //@TODO Multisampling
        };

        description_t desc;
        pipeline_layout_t layout;
      };

      struct compute_pipeline_t : pipeline_t
      {         
        shader_t computeShader;
      };


      struct vertex_attribute_t
      {
        enum format_e {
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

        format_e format;
        uint32_t offset;
        uint32_t stride;
        bool instanced;
      };

      struct vertex_format_t
      {
        VkPipelineVertexInputStateCreateInfo vertexInputState;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;

        vertex_attribute_t* attributes;
        uint32_t attributeCount;

        uint32_t vertexSize;
      };

      struct render_pass_t
      {
        struct attachment_t
        {
          VkFormat format;
          VkSampleCountFlagBits samples;
          VkImageLayout initialLayout;
          VkImageLayout finalLayout;
          VkAttachmentStoreOp storeOp;
          VkAttachmentLoadOp loadOp;
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
          std::vector<uint32_t> colorAttachmentIndex;
          std::vector<uint32_t> inputAttachmentIndex;
          int32_t depthStencilAttachmentIndex = -1;
        };

        VkRenderPass handle;

        uint32_t attachmentCount;
        attachment_t* attachment;
      };

      struct frame_buffer_t
      {
        VkFramebuffer handle;
        uint32_t width;
        uint32_t height;

        render_pass_t renderPass;
        render_pass_t renderPassNoClear;
      };

      struct combined_image_sampler_count { combined_image_sampler_count(uint32_t count) :data(count) {} uint32_t data; };
      struct uniform_buffer_count { uniform_buffer_count(uint32_t count) :data(count) {} uint32_t data; };
      struct storage_buffer_count { storage_buffer_count(uint32_t count) :data(count) {} uint32_t data; };
      struct storage_image_count { storage_image_count(uint32_t count) :data(count) {} uint32_t data; };

    }//render
  }//core
}//bkk

#endif