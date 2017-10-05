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
      enum struct filter_mode
      {
        NEAREST = 0,
        LINEAR = 1
      };

      enum struct wrap_mode
      {
        REPEAT = 0,
        MIRRORED_REPEAT = 1,
        CLAMP_TO_EDGE = 2,
        CLAMP_TO_BORDER = 3,
        MIRROR_CLAMP_TO_EDGE = 4
      };

      filter_mode minification_ = filter_mode::LINEAR;   //Minification filter (NEAREST,LINEAR)
      filter_mode magnification_ = filter_mode::LINEAR;  //Magnification filter(NEAREST,LINEAR)
      filter_mode mipmap_ = filter_mode::LINEAR;         //For trilinear interpolation (NEAREST,LINEAR)
      wrap_mode wrapU_ = wrap_mode::CLAMP_TO_EDGE;
      wrap_mode wrapV_ = wrap_mode::CLAMP_TO_EDGE;
      wrap_mode wrapW_ = wrap_mode::CLAMP_TO_EDGE;
    };    

    struct gpu_buffer_t
    {
      enum struct usage
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

      VkBuffer handle_;
      gpu_memory_t memory_;
      usage usage_;
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

    struct pipeline_layout_t
    {
      VkPipelineLayout handle_;
      uint32_t descriptorSetLayoutCount_;
      descriptor_set_layout_t* descriptorSetLayout_;
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
      enum type
      {
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
        std::vector<VkPipelineColorBlendAttachmentState> blendState_;
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
      enum format
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

      format format_;
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

    struct command_buffer_t
    {
      enum type
      {
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

      struct subpass_t
      {
        std::vector< uint32_t > colorAttachment_;
        std::vector< uint32_t > inputAttachment_;
        uint32_t depthStencilAttachment_;
      };

      VkRenderPass handle_;

      uint32_t attachmentCount_;
      attachment_t* attachment_;
      int32_t depthStencilAttachment_;
    };

    struct frame_buffer_t
    {
      VkFramebuffer handle_;
      uint32_t width_;
      uint32_t height_;

      render_pass_t renderPass_;
    };

    //Context
    void contextCreate(const char* applicationName, const char* engineName, const window::window_t& window, uint32_t swapChainImageCount, context_t* context);
    void contextDestroy(context_t* context);
    void contextFlush(const context_t& context);
    void swapchainResize(context_t* context, uint32_t width, uint32_t height);
    VkCommandBuffer beginPresentationCommandBuffer(const context_t& context, uint32_t index, VkClearValue* clearValues);
    void endPresentationCommandBuffer(const context_t& context, uint32_t index);
    void presentNextImage(context_t* context, uint32_t waitSemaphoreCount = 0u, VkSemaphore* waitSemaphore = nullptr);

    //Shaders
    bool shaderCreateFromSPIRV(const context_t& context, shader_t::type type, const char* file, shader_t* shader);
    bool shaderCreateFromGLSL(const context_t& context, shader_t::type type, const char* file, shader_t* shader);
    bool shaderCreateFromGLSLSource(const context_t& context, shader_t::type type, const char* glslSource, shader_t* shader);
    void shaderDestroy(const context_t& context, shader_t* shader);

    //GPU memory
    gpu_memory_t gpuMemoryAllocate(const context_t& context, VkDeviceSize size, VkDeviceSize alignment, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator = nullptr);
    void gpuMemoryDeallocate(const context_t& context, gpu_memory_t memory, gpu_memory_allocator_t* allocator = nullptr);
    void* gpuMemoryMap(const context_t& context, gpu_memory_t memory, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags = 0);
    void gpuMemoryUnmap(const context_t& context, gpu_memory_t memory);
    void gpuAllocatorCreate(const context_t& context, size_t size, uint32_t memoryTypes, uint32_t flags, gpu_memory_allocator_t* allocator);
    void gpuAllocatorDestroy(const context_t& context, gpu_memory_allocator_t* allocator);

    //Textures
    void texture2DCreate(const context_t& context, const image::image2D_t* images, uint32_t imageCount, texture_sampler_t sampler, texture_t* texture);
    void texture2DCreate(const context_t& context, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usageFlags, texture_sampler_t sampler, texture_t* texture);    
    void textureDestroy(const context_t& context, texture_t* texture);
    void textureChangeLayout(const context_t& context, VkCommandBuffer cmdBuffer, VkImageLayout layout, texture_t* texture);
    void textureChangeLayoutNow(const context_t& context, VkImageLayout layout, texture_t* texture);    

    //Buffers
    void gpuBufferCreate(const context_t& context, gpu_buffer_t::usage usage, uint32_t memoryType, void* data, size_t size, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator = nullptr);
    void gpuBufferCreate(const context_t& context, gpu_buffer_t::usage usage, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer);
    void gpuBufferDestroy(const context_t& context, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator = nullptr);
    void gpuBufferUpdate(const context_t& context, void* data, size_t offset, size_t size, gpu_buffer_t* buffer);
    void* gpuBufferMap(const context_t& context, const gpu_buffer_t& buffer);
    void gpuBufferUnmap(const context_t& context, const gpu_buffer_t& buffer);
    
    //Descriptors
    descriptor_t getDescriptor(const gpu_buffer_t& buffer);
    descriptor_t getDescriptor(const texture_t& texture);
    descriptor_t getDescriptor(const depth_stencil_buffer_t& depthStencilBuffer);

    void descriptorPoolCreate(const context_t& context, uint32_t descriptorSetsCount, uint32_t combinedImageSamplersCount, uint32_t uniformBuffersCount, uint32_t storageBuffersCount, uint32_t storageImagesCount, descriptor_pool_t* descriptorPool);
    void descriptorPoolDestroy(const context_t& context, descriptor_pool_t* descriptorPool);

    void descriptorSetCreate(const context_t& context, const descriptor_pool_t& descriptorPool, const descriptor_set_layout_t& descriptorSetLayout, descriptor_t* descriptors, descriptor_set_t* descriptorSet);
    void descriptorSetDestroy(const context_t& context, descriptor_set_t* descriptorSet);
    void descriptorSetUpdate(const context_t& context, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet);
    void descriptorSetBindForGraphics(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount);
    void descriptorSetBindForCompute(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount);
    void descriptorSetLayoutCreate(const context_t& context, uint32_t bindingCount, descriptor_binding_t* bindings, descriptor_set_layout_t* desriptorSetLayout);
    
    //Pipelines
    void pipelineLayoutCreate(const context_t& context, uint32_t descriptorSetLayoutCount, descriptor_set_layout_t* descriptorSetLayouts, pipeline_layout_t* pipelineLayout);
    void pipelineLayoutDestroy(const context_t& context, pipeline_layout_t* pipelineLayout);

    void graphicsPipelineCreate(const context_t& context, VkRenderPass renderPass, const render::vertex_format_t& vertexFormat, const pipeline_layout_t& pipelineLayout, const graphics_pipeline_t::description_t& pipelineDesc, graphics_pipeline_t* pipeline);
    void graphicsPipelineDestroy(const context_t& context, graphics_pipeline_t* pipeline);
    void graphicsPipelineBind( VkCommandBuffer commandBuffer, const graphics_pipeline_t& pipeline);

    void computePipelineCreate(const context_t& context, const pipeline_layout_t& pipelineLayout, compute_pipeline_t* pipeline);
    void computePipelineDestroy(const context_t& context, compute_pipeline_t* pipeline);
    void computePipelineBind( VkCommandBuffer commandBuffer, const compute_pipeline_t& pipeline);

    //Vertex formats
    void vertexFormatCreate(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format);
    void vertexFormatDestroy(vertex_format_t* format);

    //Command buffers
    void commandBufferCreate(const context_t& context, VkCommandBufferLevel level, uint32_t waitSemaphoreCount, VkSemaphore* waitSemaphore, VkPipelineStageFlags* waitStages, uint32_t signalSemaphoreCount, VkSemaphore* signalSemaphore, command_buffer_t::type type, command_buffer_t* commandBuffer);
    void commandBufferDestroy(const context_t& context, command_buffer_t* commandBuffer );
    void commandBufferBegin(const context_t& context, const frame_buffer_t* frameBuffer, uint32_t clearValuesCount, VkClearValue* clearValues, const command_buffer_t& commandBuffer);
    void commandBufferEnd(const context_t& context, const command_buffer_t& commandBuffer);
    void commandBufferSubmit(const context_t& context, const command_buffer_t& commandBuffer );

    //Renderpass
    void renderPassCreate(const context_t& context,
      uint32_t attachmentCount, render_pass_t::attachment_t* attachments,
      uint32_t subpassCount, render_pass_t::subpass_t* subpasses,
      render_pass_t* renderPass);

    void renderPassDestroy(const context_t& context, render_pass_t* renderPass);

    //FrameBuffers
    void frameBufferCreate(const context_t& context, uint32_t width, uint32_t height, const render_pass_t& renderPass, VkImageView* imageViews, frame_buffer_t* frameBuffer);
    void frameBufferDestroy(const context_t& context, frame_buffer_t* frameBuffer);
    void depthStencilBufferCreate(const context_t& context, uint32_t width, uint32_t height, depth_stencil_buffer_t* depthStencilBuffer);
    void depthStencilBufferDestroy(const context_t& context, depth_stencil_buffer_t* depthStencilBuffer);   
    void depthStencilBufferChangeLayout(const context_t& context, VkCommandBuffer cmdBuffer, VkImageLayout newLayout, depth_stencil_buffer_t* depthStencilBuffer);

  } //namespace render

}//namespace bkk
#endif // RENDER_H