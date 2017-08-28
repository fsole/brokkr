#ifndef RENDER_H
#define RENDER_H

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XCB_KHR
#endif

#include <vulkan/vulkan.h>
#include <vector>

#include "window.h"
#include "image.h"

namespace bkk
{
  namespace render
  {
    struct import_table_t
    {
      #define GET_INSTANCE_ENTRYPOINT(i, w) w = reinterpret_cast<PFN_##w>(vkGetInstanceProcAddr(i, #w))
      #define GET_DEVICE_ENTRYPOINT(i, w) w = reinterpret_cast<PFN_##w>(vkGetDeviceProcAddr(i, #w))

      void Initialize(VkInstance instance, VkDevice device)
      {
        GET_INSTANCE_ENTRYPOINT(instance, vkGetPhysicalDeviceSurfaceSupportKHR);
        GET_INSTANCE_ENTRYPOINT(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        GET_INSTANCE_ENTRYPOINT(instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
        GET_INSTANCE_ENTRYPOINT(instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
        GET_INSTANCE_ENTRYPOINT(instance, vkCreateDebugReportCallbackEXT);
        GET_INSTANCE_ENTRYPOINT(instance, vkDestroyDebugReportCallbackEXT);

        GET_DEVICE_ENTRYPOINT(device, vkCreateSwapchainKHR);
        GET_DEVICE_ENTRYPOINT(device, vkDestroySwapchainKHR);
        GET_DEVICE_ENTRYPOINT(device, vkGetSwapchainImagesKHR);
        GET_DEVICE_ENTRYPOINT(device, vkAcquireNextImageKHR);
        GET_DEVICE_ENTRYPOINT(device, vkQueuePresentKHR);
      }

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

    enum gpu_memory_type_e
    {
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
      VkImage image_;
      gpu_memory_t memory_;
      VkImageView imageView_;
    };

    struct surface_t
    {
      VkSurfaceKHR handle_;
      VkFormat imageFormat_;
      VkColorSpaceKHR colorSpace_;
      VkSurfaceTransformFlagBitsKHR preTransform_;
    };

    struct swapchain_t
    {
      VkSwapchainKHR handle_;

      uint32_t imageCount_;
      uint32_t currentImage_;
      uint32_t imageWidth_;
      uint32_t imageHeight_;

      std::vector<VkImage> image_;
      std::vector<VkImageView> imageView_;
      depth_stencil_buffer_t depthStencil_;

      std::vector<VkFramebuffer> frameBuffer_;
      std::vector<VkCommandBuffer> commandBuffer_;
      std::vector<VkFence> frameFence_;

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
      VkCommandBuffer initializationCmdBuffer_;
      queue_t graphicsQueue_;
      queue_t computeQueue_;
      surface_t surface_;
      swapchain_t swapChain_;
      import_table_t importTable_;
      VkDebugReportCallbackEXT debugCallback_;
    };

    struct texture_t
    {
      VkImage image_;
      gpu_memory_t memory_;
      VkImageView imageView_;
      VkSampler sampler_;
      VkImageLayout layout_;
      uint32_t mipLevels_;
      VkExtent3D extent_;
      VkDescriptorImageInfo descriptor_;
    };

    enum struct filter_mode_e
    {
      NEAREST = 0,
      LINEAR = 1
    };

    enum struct wrap_mode_e
    {
      REPEAT = 0,
      MIRRORED_REPEAT = 1,
      CLAMP_TO_EDGE = 2,
      CLAMP_TO_BORDER = 3,
      MIRROR_CLAMP_TO_EDGE = 4
    };

    struct texture_sampler_t
    {
      filter_mode_e minification_;   //Minification filter (NEAREST,LINEAR)
      filter_mode_e magnification_;  //Magnification filter(NEAREST,LINEAR)
      filter_mode_e mipmap_;         //For trilinear interpolation (NEAREST,LINEAR)
      wrap_mode_e wrapU_;
      wrap_mode_e wrapV_;
      wrap_mode_e wrapW_;
    };

    enum struct gpu_buffer_usage_e
    {
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

    struct gpu_buffer_t
    {
      VkBuffer handle_;
      gpu_memory_t memory_;
      gpu_buffer_usage_e usage_;
      VkDescriptorBufferInfo descriptor_;
    };

    enum struct descriptor_type_e
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

    enum descriptor_stage_e
    {
      VERTEX = 0x00000001,
      TESSELLATION_CONTROL = 0x00000002,
      TESSELLATION_EVALUATION = 0x00000004,
      GEOMETRY = 0x00000008,
      FRAGMENT = 0x00000010,
      COMPUTE = 0x00000020
    };

    struct descriptor_binding_t
    {
      descriptor_type_e type_;
      uint32_t binding_;
      uint32_t stageFlags_;
    };

    struct descriptor_set_layout_t
    {
      VkDescriptorSetLayout handle_;
      uint32_t bindingCount_;
      descriptor_binding_t* bindings_;
      //std::vector<descriptor_binding_t> bindings_;
    };

    struct pipeline_layout_t
    {
      std::vector<descriptor_set_layout_t> descriptorSetLayout_;
      VkPipelineLayout handle_;
    };

    struct descriptor_t
    {
      VkDescriptorImageInfo imageDescriptor_;
      VkDescriptorBufferInfo bufferDescriptor_;
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
      std::vector<descriptor_t> descriptors_;
      descriptor_pool_t pool_;
    };

    struct shader_t
    {
      enum type_e
      {
        VERTEX_SHADER,
        FRAGMENT_SHADER,
        TESSELLATION_SHADER,
        COMPUTE_SHADER
      };

      VkShaderModule handle_;
      type_e type_;
    };

    struct graphics_pipeline_t
    {
      VkPipeline handle_;

      VkViewport viewPort_;
      VkRect2D scissorRect_;
      std::vector<VkPipelineColorBlendAttachmentState> blendState_;
      VkCullModeFlags cullMode_;
      bool depthTestEnabled_;
      bool depthWriteEnabled_;
      VkCompareOp depthTestFunction_;
      //@TODO Stencil
      //@TODO Multisampling

      shader_t vertexShader_;
      shader_t fragmentShader_;
    };

    struct compute_pipeline_t
    {
      VkPipeline handle_;
      shader_t computeShader_;
    };

    enum attribute_format_e
    {
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
      ATTRIBUTE_FORMAT_COUNT
    };


    struct vertex_attribute_t
    {
      attribute_format_e format_;
      uint32_t offset_;
      uint32_t stride_;
    };

    struct vertex_format_t
    {
      VkPipelineVertexInputStateCreateInfo vertexInputState_;
      VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_;
      uint32_t attributeCount_;
      uint32_t vertexSize_;
    };

    void contextCreate(const char* applicationName, const char* engineName, const window::window_t& window, uint32_t swapChainImageCount, context_t* context);
    void contextDestroy(context_t* context);
    void contextFlush(const context_t& context);
    void initResources(const context_t& context);
    void swapchainResize(context_t* context, uint32_t width, uint32_t height);
    VkCommandBuffer beginPresentationCommandBuffer(const context_t& context, uint32_t index, VkClearValue* clearValues);
    void endPresentationCommandBuffer(const context_t& context, uint32_t index);
    void presentNextImage(context_t* context);

    bool shaderCreateFromSPIRV(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader);
    bool shaderCreateFromGLSL(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader);
    bool shaderCreateFromGLSLSource(const context_t& context, shader_t::type_e type, const char* glslSource, shader_t* shader);
    void shaderDestroy(const context_t& context, shader_t* shader);

    gpu_memory_t gpuMemoryAllocate(const context_t& context, VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator = nullptr);
    void gpuMemoryDeallocate(const context_t& context, gpu_memory_t memory, gpu_memory_allocator_t* allocator = nullptr);
    void* gpuMemoryMap(const context_t& context, gpu_memory_t memory, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags = 0);
    void gpuMemoryUnmap(const context_t& context, gpu_memory_t memory);

    void gpuAllocatorCreate(const context_t& context, size_t size, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator);
    void gpuAllocatorDestroy(const context_t& context, gpu_memory_allocator_t* allocator);

    void texture2DCreate(const context_t& context, const image::image2D_t* images, uint32_t imageCount, texture_sampler_t sampler, texture_t* texture);
    void texture2DCreate(const context_t& context, uint32_t width, uint32_t height, uint32_t componentCount, uint32_t usageFlags, texture_sampler_t sampler, texture_t* texture);
    void textureDestroy(const context_t& context, texture_t* texture);
    void textureChangeLayout(const context_t& context, VkCommandBuffer cmdBuffer, VkImageLayout layout, VkImageAspectFlags aspect, texture_t* texture);

    void gpuBufferCreate(const context_t& context, gpu_buffer_usage_e usage, uint32_t memoryType, void* data, size_t size, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator = nullptr);
    void gpuBufferCreate(const context_t& context, gpu_buffer_usage_e usage, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer);
    void gpuBufferDestroy(const context_t& context, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator = nullptr);
    void gpuBufferUpdate(const context_t& context, void* data, size_t offset, size_t size, gpu_buffer_t* buffer);
    void* gpuBufferMap(const context_t& context, const gpu_buffer_t& buffer);
    void gpuBufferUnmap(const context_t& context, const gpu_buffer_t& buffer);


    void descriptorSetLayoutCreate(const context_t& context, uint32_t bindingCount, descriptor_binding_t* bindings, descriptor_set_layout_t* desriptorSetLayout);
    void pipelineLayoutCreate(const context_t& context, pipeline_layout_t* pipelineLayout);
    void pipelineLayoutDestroy(const context_t& context, pipeline_layout_t* pipelineLayout);

    void descriptorPoolCreate(const context_t& context, descriptor_pool_t* descriptorPool);
    void descriptorPoolDestroy(const context_t& context, descriptor_pool_t* descriptorPool);
    
    void descriptorSetCreate(const context_t& context, const descriptor_pool_t& descriptorPool, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet);
    void descriptorSetDestroy(const context_t& context, descriptor_set_t* descriptorSet);
    void descriptorSetUpdate(const context_t& context, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet);
    void descriptorSetBindForGraphics(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount);
    void descriptorSetBindForCompute(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount);

    void graphicsPipelineCreate(const context_t& context, VkRenderPass renderPass, const render::vertex_format_t& vertexFormat, const pipeline_layout_t& pipelineLayout, graphics_pipeline_t* pipeline);
    void graphicsPipelineDestroy(const context_t& context, graphics_pipeline_t* pipeline);
    void graphicsPipelineBind( VkCommandBuffer commandBuffer, const graphics_pipeline_t& pipeline);

    void computePipelineCreate(const context_t& context, const pipeline_layout_t& pipelineLayout, compute_pipeline_t* pipeline);
    void computePipelineDestroy(const context_t& context, compute_pipeline_t* pipeline);
    void computePipelineBind( VkCommandBuffer commandBuffer, const compute_pipeline_t& pipeline);

    void allocateCommandBuffers(const context_t& context, VkCommandBufferLevel level, uint32_t count, VkCommandBuffer* buffers);
    void freeCommandBuffers(const context_t& context, uint32_t count, VkCommandBuffer* buffers);

    void vertexFormatCreate(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format);
    void vertexFormatDestroy(vertex_format_t* format);

  } //namespace render

}//namespace bkk
#endif // RENDER_H