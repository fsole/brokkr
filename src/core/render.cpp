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

#include "core/render.h"
#include "core/mesh.h"
#include "core/window.h"
#include "core/image.h"

#include <stdio.h>
#include <assert.h>
#include <string>

using namespace bkk::core;
using namespace bkk::core::render;


#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objType,
  uint64_t obj,
  size_t location,
  int32_t code,
  const char* layerPrefix,
  const char* msg,
  void* userData)
{
  fprintf(stderr, "VULKAN_ERROR: %s \n", msg );
  return VK_FALSE;
}
  
static VkDeviceSize GetNextMultiple(VkDeviceSize from, VkDeviceSize multiple)
{
  return ((from + multiple - 1) / multiple) * multiple;
}

static int32_t GetQueueIndex(const VkPhysicalDevice* physicalDevice, VkQueueFlagBits queueType)
{
  //Get number of queue families
  uint32_t queueFamilyPropertyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(*physicalDevice, &queueFamilyPropertyCount, nullptr);

  //Get properties of each family
  std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(*physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

  for (uint32_t queueIndex = 0; queueIndex < queueFamilyPropertyCount; ++queueIndex)
  {
    //If queue supports graphics operations return physical device and queue index
    if (queueFamilyProperties[queueIndex].queueFlags & queueType)
    {
      return queueIndex;
    }
  }

  //NO graphics queue found in the device
  return -1;
}

static VkRenderPass CreatePresentationRenderPass(VkDevice device, VkFormat imageFormat, VkFormat depthStencilFormat)
{
  VkAttachmentDescription attachmentDescription[2] = {};
  attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription[0].format = imageFormat;
  attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  attachmentDescription[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachmentDescription[1].format = depthStencilFormat;
  attachmentDescription[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachmentDescription[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachmentDescription[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachmentDescription[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  attachmentDescription[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachmentDescription[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  VkAttachmentReference attachmentColorReference = {};
  attachmentColorReference.attachment = 0;
  attachmentColorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference attachmentDepthStencilReference = {};
  attachmentDepthStencilReference.attachment = 1;
  attachmentDepthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpassDescription = {};
  subpassDescription.inputAttachmentCount = 0;
  subpassDescription.pColorAttachments = &attachmentColorReference;
  subpassDescription.colorAttachmentCount = 1;
  subpassDescription.pDepthStencilAttachment = &attachmentDepthStencilReference;
  subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  VkRenderPassCreateInfo renderPassCreateInfo = {};
  renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassCreateInfo.attachmentCount = 2;
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpassDescription;
  renderPassCreateInfo.pAttachments = attachmentDescription;

  VkRenderPass result;
  vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &result);

  return result;
}

static VkInstance CreateInstance(const char* applicationName, const char* engineName)
{
  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;


  std::vector<const char*> instanceExtensions;
  instanceExtensions.push_back("VK_KHR_surface");

#ifdef WIN32
  instanceExtensions.push_back("VK_KHR_win32_surface");
#else
  instanceExtensions.push_back("VK_KHR_xcb_surface");
#endif
  std::vector<const char*> instanceLayers;


#ifdef VK_DEBUG_LAYERS
  instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
  instanceExtensions.push_back("VK_EXT_debug_report");
#endif

  //Enable extensions and layers
  instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
  instanceCreateInfo.ppEnabledLayerNames = instanceLayers.data();
  instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());

  VkApplicationInfo applicationInfo = {};
  applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  applicationInfo.apiVersion = VK_API_VERSION_1_0;
  applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  applicationInfo.pApplicationName = "brokkr Application";
  applicationInfo.pEngineName = "brokkr Framework";
  instanceCreateInfo.pApplicationInfo = &applicationInfo;

  VkInstance instance = VK_NULL_HANDLE;
  vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
  assert(instance);

  return instance;
}

static void CreateDeviceAndQueues(VkInstance instance,
  VkPhysicalDevice* physicalDevice,
  VkDevice* logicalDevice,
  queue_t* graphicsQueue,
  queue_t* computeQueue)
{
  uint32_t physicalDeviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

  std::vector<VkPhysicalDevice> devices( physicalDeviceCount);
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data());

  //Find a physical device with graphics and compute queues
  *physicalDevice = nullptr;
  graphicsQueue->queueIndex = -1;
  computeQueue->queueIndex  = -1;
  for (uint32_t i(0); i < physicalDeviceCount; ++i)
  {
    graphicsQueue->queueIndex  = GetQueueIndex(&devices[i], VK_QUEUE_GRAPHICS_BIT);
    computeQueue->queueIndex  = GetQueueIndex(&devices[i], VK_QUEUE_COMPUTE_BIT);
    if (graphicsQueue->queueIndex  != -1 && computeQueue->queueIndex != -1)
    {
      *physicalDevice = devices[i];
      break;
    }
  }

  assert(*physicalDevice);

  VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
  deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfo.queueCount = 1;
  deviceQueueCreateInfo.queueFamilyIndex = graphicsQueue->queueIndex ;

  static const float queuePriorities[] = { 1.0f };
  deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

  deviceCreateInfo.ppEnabledLayerNames = NULL;
  deviceCreateInfo.enabledLayerCount = 0u;

  const char* deviceExtensions = "VK_KHR_swapchain";

  deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensions;
  deviceCreateInfo.enabledExtensionCount = 1u;

  *logicalDevice = nullptr;
  vkCreateDevice(*physicalDevice, &deviceCreateInfo, nullptr, logicalDevice);
  assert(*logicalDevice);

  graphicsQueue->handle = nullptr;
  vkGetDeviceQueue(*logicalDevice, graphicsQueue->queueIndex, 0, &graphicsQueue->handle);
  assert(graphicsQueue->handle);

  computeQueue->handle = nullptr;
  vkGetDeviceQueue(*logicalDevice, computeQueue->queueIndex, 0, &computeQueue->handle);
  assert(computeQueue->handle);
}

static VkBool32 GetDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
{
  VkFormat depthFormats[] = { VK_FORMAT_D32_SFLOAT_S8_UINT,
                              VK_FORMAT_D32_SFLOAT,
                              VK_FORMAT_D24_UNORM_S8_UINT,
                              VK_FORMAT_D16_UNORM_S8_UINT,
                              VK_FORMAT_D16_UNORM };

  for (size_t i(0); i < 5; ++i)
  {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, depthFormats[i], &formatProps);
    if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
    {
      *depthFormat = depthFormats[i];
      return true;
    }
  }

  return false;
}


static void CreateDepthStencilBuffer(const context_t* context, uint32_t width, uint32_t height, VkFormat format, depth_stencil_buffer_t* depthStencilBuffer)
{
  depthStencilBuffer->format = format;

  //Create the image
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.format = format;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.extent = { width, height, 1u };
  imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.flags = 0;
  vkCreateImage(context->device, &imageCreateInfo, nullptr, &depthStencilBuffer->image);

  //Allocate and bind memory for the image.
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context->device, depthStencilBuffer->image, &requirements);
  depthStencilBuffer->memory = gpuMemoryAllocate(*context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context->device, depthStencilBuffer->image, depthStencilBuffer->memory.handle, depthStencilBuffer->memory.offset);

  //Create command buffer
  VkCommandBuffer commandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context->commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo, &commandBuffer);

  //Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  //Transition image layout from undefined to depth-stencil
  VkImageMemoryBarrier imageBarrier = {};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageBarrier.pNext = nullptr;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  imageBarrier.srcAccessMask = 0;
  imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  imageBarrier.image = depthStencilBuffer->image;
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 1;
  imageBarrier.subresourceRange = subresourceRange;

  vkCmdPipelineBarrier(commandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);

  //End command buffer
  vkEndCommandBuffer(commandBuffer);

  ////Queue commandBuffer for execution
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  vkResetFences(context->device, 1u, &context->swapChain.commandBuffer[0].fence);
  vkQueueSubmit(context->graphicsQueue.handle, 1, &submitInfo, context->swapChain.commandBuffer[0].fence);

  //Destroy command buffer
  vkWaitForFences(context->device, 1u, &context->swapChain.commandBuffer[0].fence, VK_TRUE, UINT64_MAX);
  vkFreeCommandBuffers(context->device, context->commandPool, 1, &commandBuffer);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.pNext = NULL;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  imageViewCreateInfo.format = format;
  imageViewCreateInfo.flags = 0;
  imageViewCreateInfo.subresourceRange = {};
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.image = depthStencilBuffer->image;
  vkCreateImageView(context->device, &imageViewCreateInfo, nullptr, &depthStencilBuffer->imageView);
}


static void CreateSurface(VkInstance instance, VkPhysicalDevice physicalDevice, const window::window_t& window, const context_t& context, surface_t* surface)
{
#ifdef WIN32
  VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surfaceCreateInfo.hinstance = window.instance;
  surfaceCreateInfo.hwnd = window.handle;
  vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface->handle);
#else
  VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  surfaceCreateInfo.connection = window->connection_;
  surfaceCreateInfo.window = window->handle;
  vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface->handle);
#endif


  //Check if presentation is supported by the surface
  VkBool32 presentSupported;
  context.vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, 0, surface->handle, &presentSupported);
  assert(presentSupported);

  //Get surface capabilities
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface->handle, &surfaceCapabilities);

  if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
  {
    surface->preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }
  else
  {
    surface->preTransform = surfaceCapabilities.currentTransform;
  }

  //Get surface format and color space
  uint32_t surfaceFormatCount = 0;
  context.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->handle, &surfaceFormatCount, nullptr);
  assert(surfaceFormatCount > 0);
  std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
  context.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->handle, &surfaceFormatCount, surfaceFormats.data());

  surface->imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
  if (surfaceFormats[0].format != VK_FORMAT_UNDEFINED)
  {
    surface->imageFormat = surfaceFormats[0].format;
  }

  surface->colorSpace = surfaceFormats.front().colorSpace;
}

static void CreateSwapChain(context_t* context,
  uint32_t width, uint32_t height,
  uint32_t imageCount)
{
  //Create semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &context->swapChain.imageAcquired);
  vkCreateSemaphore(context->device, &semaphoreCreateInfo, nullptr, &context->swapChain.renderingComplete);

  VkExtent2D swapChainSize = { width, height };
  context->swapChain.imageWidth = width;
  context->swapChain.imageHeight = height;
  context->swapChain.imageCount = imageCount;
  context->swapChain.currentImage = 0;

  //Create the swapchain
  VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface = context->surface.handle;
  swapchainCreateInfo.minImageCount = imageCount;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchainCreateInfo.imageColorSpace = context->surface.colorSpace;
  swapchainCreateInfo.imageFormat = context->surface.imageFormat;
  swapchainCreateInfo.pQueueFamilyIndices = nullptr;
  swapchainCreateInfo.queueFamilyIndexCount = 0;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.imageExtent = swapChainSize;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  context->vkCreateSwapchainKHR(context->device, &swapchainCreateInfo, nullptr, &context->swapChain.handle);

  //Get the maximum number of images supported by the swapchain
  uint32_t maxImageCount = 0;
  context->vkGetSwapchainImagesKHR(context->device, context->swapChain.handle, &maxImageCount, nullptr);

  //Create the swapchain images
  assert(imageCount <= maxImageCount);
  context->swapChain.image.resize(imageCount);
  context->vkGetSwapchainImagesKHR(context->device, context->swapChain.handle, &maxImageCount, context->swapChain.image.data());

  //Create an imageview and one command buffer for each image
  context->swapChain.imageView.resize(imageCount);
  context->swapChain.commandBuffer.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = context->swapChain.image[i];
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = context->surface.imageFormat;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCreateImageView(context->device, &imageViewCreateInfo, nullptr, &context->swapChain.imageView[i]);

    commandBufferCreate(*context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u,
      &context->swapChain.renderingComplete, 1u, command_buffer_t::GRAPHICS,
      &context->swapChain.commandBuffer[i]);

  }

  //Create depth stencil buffer (shared by all the framebuffers)
  VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
  GetDepthStencilFormat(context->physicalDevice, &depthStencilFormat);
  CreateDepthStencilBuffer(context, width, height, depthStencilFormat, &context->swapChain.depthStencil);

  //Create the presentation render pass
  context->swapChain.renderPass = CreatePresentationRenderPass(context->device, context->surface.imageFormat, depthStencilFormat);

  //Create frame buffers
  context->swapChain.frameBuffer.resize(imageCount);
  VkImageView attachments[2];
  attachments[1] = context->swapChain.depthStencil.imageView;
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    attachments[0] = context->swapChain.imageView[i];
    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.attachmentCount = 2;
    framebufferCreateInfo.pAttachments = attachments;
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.renderPass = context->swapChain.renderPass;

    vkCreateFramebuffer(context->device, &framebufferCreateInfo, nullptr, &context->swapChain.frameBuffer[i]);
  }
}

static VkCommandPool CreateCommandPool(VkDevice device, uint32_t queueIndex)
{
  VkCommandPool pool;
  VkCommandPoolCreateInfo commandPoolCreateInfo = {};
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = queueIndex;
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &pool);

  return pool;
}


static void ImportFunctions(VkInstance instance, VkDevice device, context_t* context )
{
  context->vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
  context->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
  context->vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
  context->vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
  context->vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT"));
  context->vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT"));

  context->vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
  context->vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
  context->vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
  context->vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
  context->vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
}

/*********************
* API Implementation
**********************/

void render::contextCreate(const char* applicationName,
  const char* engineName,
  const window::window_t& window,
  uint32_t swapChainImageCount,
  context_t* context)
{
  context->instance = CreateInstance(applicationName, engineName);
  CreateDeviceAndQueues(context->instance, &context->physicalDevice, &context->device, &context->graphicsQueue, &context->computeQueue);

  //Get memory properties of the physical device
  vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &context->memoryProperties);

  context->commandPool = CreateCommandPool(context->device, context->graphicsQueue.queueIndex);
  
  ImportFunctions(context->instance, context->device, context);

#ifdef VK_DEBUG_LAYERS
  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
  createInfo.pfnCallback = debugCallback;
  context->vkCreateDebugReportCallbackEXT(context->instance, &createInfo, nullptr, &context->debugCallback);
#endif

  CreateSurface(context->instance, context->physicalDevice, window, *context, &context->surface);

  CreateSwapChain(context, window.width, window.height, swapChainImageCount);
}

void render::contextDestroy(context_t* context)
{
  vkDestroySemaphore(context->device, context->swapChain.imageAcquired, nullptr);
  vkDestroySemaphore(context->device, context->swapChain.renderingComplete, nullptr);

  for (uint32_t i = 0; i < context->swapChain.imageCount; ++i)
  {
    vkDestroyFramebuffer(context->device, context->swapChain.frameBuffer[i], nullptr);
    vkDestroyImageView(context->device, context->swapChain.imageView[i], nullptr);
    commandBufferDestroy(*context, &context->swapChain.commandBuffer[i]);
  }

  //Destroy depthstencil buffer
  vkDestroyImageView(context->device, context->swapChain.depthStencil.imageView, nullptr);
  vkDestroyImage(context->device, context->swapChain.depthStencil.image, nullptr);
  gpuMemoryDeallocate(*context, nullptr, context->swapChain.depthStencil.memory);

  vkDestroyCommandPool(context->device, context->commandPool, nullptr);
  vkDestroyRenderPass(context->device, context->swapChain.renderPass, nullptr);
  vkDestroySwapchainKHR(context->device, context->swapChain.handle, nullptr);
  vkDestroySurfaceKHR(context->instance, context->surface.handle, nullptr);

#ifdef VK_DEBUG_LAYERS
  context->vkDestroyDebugReportCallbackEXT(context->instance, context->debugCallback, nullptr);
#endif

  vkDestroyDevice(context->device, nullptr);
  vkDestroyInstance(context->instance, nullptr);
}


void render::swapchainResize(context_t* context, uint32_t width, uint32_t height)
{
  //TODO: Handle width and height equal 0!
  contextFlush(*context);

  //Destroy framebuffers and command buffers
  for (uint32_t i = 0; i < context->swapChain.imageCount; ++i)
  {
    vkDestroyFramebuffer(context->device, context->swapChain.frameBuffer[i], nullptr);
    vkDestroyImageView(context->device, context->swapChain.imageView[i], nullptr);
    commandBufferDestroy(*context, &context->swapChain.commandBuffer[i]);
  }
  
  //Destroy depthstencil buffer
  vkDestroyImageView(context->device, context->swapChain.depthStencil.imageView, nullptr);
  vkDestroyImage(context->device, context->swapChain.depthStencil.image, nullptr);
  gpuMemoryDeallocate(*context, nullptr, context->swapChain.depthStencil.memory);

  //Recreate swapchain with the new size
  vkDestroyRenderPass(context->device, context->swapChain.renderPass, nullptr);
  vkDestroySwapchainKHR(context->device, context->swapChain.handle, nullptr);
  CreateSwapChain(context, width, height, context->swapChain.imageCount);

}

void render::contextFlush(const context_t& context)
{
  for (u32 i = 0; i < context.swapChain.imageCount; ++i)
  {
    vkWaitForFences(context.device, 1u, &context.swapChain.commandBuffer[i].fence, VK_TRUE, UINT64_MAX);
  }
  vkQueueWaitIdle(context.graphicsQueue.handle);
  vkQueueWaitIdle(context.computeQueue.handle);
}

void render::beginPresentationCommandBuffer(const context_t& context, uint32_t index, VkClearValue* clearValues)
{
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

  renderPassBeginInfo.renderArea.extent = { context.swapChain.imageWidth,context.swapChain.imageHeight };
  renderPassBeginInfo.renderPass = context.swapChain.renderPass;

  if (clearValues)
  {
    renderPassBeginInfo.pClearValues = clearValues;
  }
  else
  {
    static VkClearValue clearValues[2];
    clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f,0 };
    renderPassBeginInfo.pClearValues = clearValues;
  }
  renderPassBeginInfo.clearValueCount = 2;

  vkWaitForFences(context.device, 1, &context.swapChain.commandBuffer[index].fence, VK_TRUE, UINT64_MAX);

  //Begin command buffer
  vkBeginCommandBuffer(context.swapChain.commandBuffer[index].handle, &beginInfo);

  //Begin render pass
  renderPassBeginInfo.framebuffer = context.swapChain.frameBuffer[index];
  vkCmdBeginRenderPass(context.swapChain.commandBuffer[index].handle, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  //Set viewport and scissor rectangle
  VkViewport viewPort = { 0.0f, 0.0f, (float)context.swapChain.imageWidth, (float)context.swapChain.imageHeight, 0.0f, 1.0f };
  VkRect2D scissorRect = { {0,0},{context.swapChain.imageWidth,context.swapChain.imageHeight} };
  vkCmdSetViewport(context.swapChain.commandBuffer[index].handle, 0, 1, &viewPort);
  vkCmdSetScissor(context.swapChain.commandBuffer[index].handle, 0, 1, &scissorRect);
}

uint32_t render::getPresentationCommandBuffers(const context_t& context, const command_buffer_t** commandBuffers)
{
  *commandBuffers = context.swapChain.commandBuffer.data();
  return (uint32_t)context.swapChain.commandBuffer.size();
}

uint32_t render::getPresentationCommandBuffer(context_t& context, command_buffer_t** commandBuffer)
{
  *commandBuffer = &context.swapChain.commandBuffer[context.swapChain.currentImage];
  return context.swapChain.currentImage;
}

void render::endPresentationCommandBuffer(const context_t& context, uint32_t index)
{
  vkCmdEndRenderPass(context.swapChain.commandBuffer[index].handle);
  vkEndCommandBuffer(context.swapChain.commandBuffer[index].handle);
}

void render::presentFrame(context_t* context, VkSemaphore* waitSemaphore, uint32_t waitSemaphoreCount)
{
  //Aquire next image in the swapchain
  context->vkAcquireNextImageKHR(context->device,
    context->swapChain.handle,
    UINT64_MAX, context->swapChain.imageAcquired,
    VK_NULL_HANDLE, &context->swapChain.currentImage);

  uint32_t currentImage = context->swapChain.currentImage;

  //Submit current command buffer  
  std::vector<VkSemaphore> waitSemaphoreList(1 + waitSemaphoreCount);
  std::vector<VkPipelineStageFlags> waitStageList(1 + waitSemaphoreCount);
  waitSemaphoreList[0] = context->swapChain.imageAcquired;
  waitStageList[0] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  for (uint32_t i(0); i < waitSemaphoreCount; ++i)
  {
    waitSemaphoreList[i+1] = waitSemaphore[i];
    waitStageList[i+1] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
  
  VkSubmitInfo submitInfo = {};  
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphoreList.size();
  submitInfo.pWaitSemaphores = waitSemaphoreList.data();	      //Wait until image is aquired
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = &context->swapChain.renderingComplete;	//When command buffer has finished will signal renderingCompleteSemaphore
  submitInfo.pWaitDstStageMask = waitStageList.data();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &context->swapChain.commandBuffer[currentImage].handle;
  vkQueueSubmit(context->graphicsQueue.handle, 1, &submitInfo, VK_NULL_HANDLE);
  
  //Present the image
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &context->swapChain.renderingComplete;	//Wait until rendering has finished
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &context->swapChain.handle;
  presentInfo.pImageIndices = &currentImage;
  context->vkQueuePresentKHR(context->graphicsQueue.handle, &presentInfo);

  //Submit presentation
  vkResetFences(context->device, 1, &context->swapChain.commandBuffer[currentImage].fence);
  vkQueueSubmit(context->graphicsQueue.handle, 0, nullptr, context->swapChain.commandBuffer[currentImage].fence);
  vkWaitForFences(context->device, 1, &context->swapChain.commandBuffer[currentImage].fence, VK_TRUE, UINT64_MAX);
}

bool render::shaderCreateFromSPIRV(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader)
{
  shader->handle = VK_NULL_HANDLE;
  shader->type = type;

  size_t size;
  FILE *fp = fopen(file, "rb");
  if (!fp)
  {
    return false;
  }

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  char* code = new char[size];
  if (fread(code, size, 1, fp) != 1)
  {
    return false;
  }

  fclose(fp);

  VkShaderModuleCreateInfo shaderCreateInfo;
  shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderCreateInfo.pNext = NULL;
  shaderCreateInfo.codeSize = size;
  shaderCreateInfo.pCode = (uint32_t*)code;
  shaderCreateInfo.flags = 0;
  VkResult result = vkCreateShaderModule(context.device, &shaderCreateInfo, NULL, &shader->handle);
  delete[] code;

  return result == VK_SUCCESS;
}

bool render::shaderCreateFromGLSL(const context_t& context, shader_t::type_e type, const char* file, shader_t* shader)
{
  std::string spirv_file_path = "temp.spv";
  std::string glslangvalidator_params = "arg0 -V -s -o \"" + spirv_file_path + "\" \"" + file + "\"";
  
  #ifdef VK_DEBUG_LAYERS
    glslangvalidator_params = "arg0 -V -o \""; 
    glslangvalidator_params += spirv_file_path + "\" \"" + file + "\"";
  #endif
  
  PROCESS_INFORMATION process_info;
  memset(&process_info, 0, sizeof(process_info));

  STARTUPINFOA startup_info;
  memset(&startup_info, 0, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);

  if (CreateProcessA("..\\..\\external\\vulkan\\bin\\win\\glslangValidator.exe",
    (LPSTR)glslangvalidator_params.c_str(),
    nullptr, nullptr, FALSE,
    CREATE_DEFAULT_ERROR_MODE,
    nullptr, nullptr,
    &startup_info,
    &process_info))
  {
    if (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_OBJECT_0)
    {
      bool result = shaderCreateFromSPIRV(context, type, spirv_file_path.c_str(), shader);
      DeleteFileA((LPCSTR)spirv_file_path.c_str());
      return result;
    }
  }

  return false;
}

bool render::shaderCreateFromGLSLSource(const context_t& context, shader_t::type_e type, const char* glslSource, shader_t* shader)
{
  std::string glslTempFile;
  switch (type)
  {
  case shader_t::VERTEX_SHADER: glslTempFile = "temp.vert";
    break;

  case shader_t::FRAGMENT_SHADER: glslTempFile = "temp.frag";
    break;

  case shader_t::COMPUTE_SHADER: glslTempFile = "temp.comp";
    break;

  default: assert(true);
    break;
  }

  FILE *fp = fopen(glslTempFile.c_str(), "ab");
  if (fp != NULL)
  {
    fputs(glslSource, fp);
    fclose(fp);
  }

  bool result = shaderCreateFromGLSL(context, type, glslTempFile.c_str(), shader);
  DeleteFileA((LPCSTR)glslTempFile.c_str());
  return result;
}

void render::shaderDestroy(const context_t& context, shader_t* shader)
{
  vkDestroyShaderModule(context.device, shader->handle, nullptr);
}

gpu_memory_t render::gpuMemoryAllocate(const context_t& context,
  VkDeviceSize size, VkDeviceSize alignment,
  uint32_t memoryTypes, uint32_t flags,
  gpu_memory_allocator_t* allocator)
{
  gpu_memory_t result = { VK_NULL_HANDLE, 0u, 0u };

  if (allocator == nullptr)
  {
    VkMemoryPropertyFlags properties = (flags & HOST_VISIBLE ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : 0) |
      (flags & DEVICE_LOCAL ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : 0) |
      (flags & HOST_COHERENT ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0);

    for (uint32_t i = 0; i < context.memoryProperties.memoryTypeCount; i++)
    {
      if ((CHECK_BIT(memoryTypes, i)) && ((context.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
      {
        //Try allocating the memory
        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.memoryTypeIndex = i;
        memoryAllocateInfo.allocationSize = size;
        VkDeviceMemory memory;
        if (vkAllocateMemory(context.device, &memoryAllocateInfo, nullptr, &memory) == VK_SUCCESS)
        {
          result.handle = memory;
          result.size = size;
          return result;
        }
      }
    }
  }
  else
  {
    VkDeviceSize offset = GetNextMultiple(allocator->head, alignment);
    if (size <= allocator->size - offset)
    {
      result.handle = allocator->memory;
      result.size = size;
      result.offset = offset;
      allocator->head = offset + size;
    }
  }

  return result;
}

void render::gpuMemoryDeallocate(const context_t& context, gpu_memory_allocator_t* allocator, gpu_memory_t memory)
{
  if (allocator == nullptr)
  {
    vkFreeMemory(context.device, memory.handle, nullptr);
  }
  else
  {
    // TODO
  }
}

void* render::gpuMemoryMap(const context_t& context, gpu_memory_t memory)
{
  void* result = nullptr;
  vkMapMemory(context.device, memory.handle, memory.offset, memory.size, 0u, &result);
  return result;
}

void* render::gpuMemoryMap(const context_t& context, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, gpu_memory_t memory)
{
  if (size == VK_WHOLE_SIZE)
  {
    size = memory.size - offset;
  }

  void* result = nullptr;
  vkMapMemory(context.device, memory.handle, memory.offset + offset, size, flags, &result);
  return result;
}


void render::gpuMemoryUnmap(const context_t& context, gpu_memory_t memory)
{
  vkUnmapMemory(context.device, memory.handle);
}


void render::gpuAllocatorCreate(const context_t& context, size_t size,
  uint32_t memoryTypes, uint32_t flags,
  gpu_memory_allocator_t* allocator)
{
  gpu_memory_t memory = gpuMemoryAllocate(context, size, 0u, memoryTypes, flags);
  allocator->memory = memory.handle;
  allocator->size = size;
  allocator->head = 0;
}

void render::gpuAllocatorDestroy(const context_t& context, gpu_memory_allocator_t* allocator)
{
  vkFreeMemory(context.device, allocator->memory, nullptr);
}

static VkFormat getImageFormat(const image::image2D_t& image)
{
  VkFormat format = VK_FORMAT_UNDEFINED;
  if (image.componentCount == 3)
  {
    if (image.componentSize == 4)
    {
      format = VK_FORMAT_R32G32B32_SFLOAT;
    }
    else
    {
      format = VK_FORMAT_R8G8B8_UNORM;
    }
  }
  else if (image.componentCount == 4)
  {
    if (image.componentSize == 4)
    {
      format = VK_FORMAT_R32G32B32A32_SFLOAT;
    }
    else
    {
      format = VK_FORMAT_R8G8B8A8_UNORM;
    }
  }

  return format;
}

void render::texture2DCreate(const context_t& context, const image::image2D_t* images, uint32_t imageCount, texture_sampler_t sampler, texture_t* texture)
{
  //Get base level image width and height
  VkExtent3D extents = { images[0].width, images[0].height, 1u };
  VkFormat format = getImageFormat(images[0]);

  //Create the image
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.mipLevels = imageCount;
  imageCreateInfo.format = format;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.extent = extents;
  imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkCreateImage(context.device, &imageCreateInfo, nullptr, &texture->image);


  //Allocate and bind memory for the image.
  //note: Memory for the image is not host visible so we will need a host visible buffer to transfer the data
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device, texture->image, &requirements);
  texture->memory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context.device, texture->image, texture->memory.handle, texture->memory.offset);

  //Upload data to the texture using an staging buffer

  //Create a staging buffer and memory store for the buffer
  VkBuffer stagingBuffer;


  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.pNext = nullptr;
  bufferCreateInfo.size = requirements.size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(context.device, &bufferCreateInfo, nullptr, &stagingBuffer);

  //Allocate and bind memory to the buffer
  vkGetBufferMemoryRequirements(context.device, stagingBuffer, &requirements);
  gpu_memory_t stagingBufferMemory;
  stagingBufferMemory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, HOST_VISIBLE);
  vkBindBufferMemory(context.device, stagingBuffer, stagingBufferMemory.handle, stagingBufferMemory.offset);

  //Map buffer memory and copy image data
  unsigned char* mapping = (unsigned char*)gpuMemoryMap(context, stagingBufferMemory);
  if (mapping)
  {
    for (uint32_t i(0); i < imageCount; ++i)
    {
      memcpy(mapping, images[i].data, images[i].dataSize);
      mapping += images[i].dataSize;
    }

    gpuMemoryUnmap(context, stagingBufferMemory);
  }

  //Create command buffer
  VkCommandBuffer uploadCommandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &uploadCommandBuffer);

  //Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(uploadCommandBuffer, &beginInfo);

  ////Copy data from the buffer to the image
  //Transition image layout from undefined to optimal-for-transfer-destination
  VkImageMemoryBarrier imageBarrier = {};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageBarrier.pNext = nullptr;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.srcAccessMask = 0;
  imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageBarrier.image = texture->image;
  imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.layerCount = 1;
  imageBarrier.subresourceRange.levelCount = 1;

  vkCmdPipelineBarrier(uploadCommandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);

  //Copy from buffer to image
  //@ToDO : Copy mipmaps. Currently only base level is copied to the image
  VkBufferImageCopy bufferImageCopy = {};
  bufferImageCopy.imageExtent = extents;
  bufferImageCopy.bufferOffset = 0;
  bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  bufferImageCopy.imageSubresource.mipLevel = 0;
  bufferImageCopy.imageSubresource.layerCount = 1;
  vkCmdCopyBufferToImage(uploadCommandBuffer, stagingBuffer,
    texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &bufferImageCopy);

  //Transition image layout from optimal-for-transfer to optimal-for-shader-reads
  imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  vkCmdPipelineBarrier(uploadCommandBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);

  //End command buffer
  vkEndCommandBuffer(uploadCommandBuffer);

  ////Queue uploadCommandBuffer for execution
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &uploadCommandBuffer;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkFence fence;
  vkCreateFence(context.device, &fenceCreateInfo, nullptr, &fence);
  vkResetFences(context.device, 1u, &fence);
  vkQueueSubmit(context.graphicsQueue.handle, 1, &submitInfo, fence);

  //Destroy temporary resources
  vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(context.device, fence, nullptr);
  vkFreeCommandBuffers(context.device, context.commandPool, 1, &uploadCommandBuffer);
  gpuMemoryDeallocate(context, nullptr, stagingBufferMemory);
  vkDestroyBuffer(context.device, stagingBuffer, nullptr);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.format = imageCreateInfo.format;
  imageViewCreateInfo.image = texture->image;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &texture->imageView);

  //Create sampler
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)sampler.magnification;
  samplerCreateInfo.minFilter = (VkFilter)sampler.minification;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)sampler.mipmap;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)sampler.wrapU;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)sampler.wrapV;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)sampler.wrapW;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = imageCount - 1.0f;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device, &samplerCreateInfo, nullptr, &texture->sampler);

  
  texture->descriptor = {};
  texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  texture->descriptor.imageView = texture->imageView;
  texture->descriptor.sampler = texture->sampler;
  texture->layout = VK_IMAGE_LAYOUT_GENERAL;
  texture->extent = extents;
  texture->mipLevels = 1;
  texture->aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  texture->format = format;

  textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture);
}

void render::texture2DCreate(const context_t& context,
  uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage,
  texture_sampler_t sampler, texture_t* texture)
{
  VkExtent3D extents = { width, height, 1u };
  
  VkImageAspectFlags aspectFlags = 0;
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
  {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  else
  {
    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  //Create the image
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.mipLevels = mipLevels;
  imageCreateInfo.format = format;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.extent = extents;
  imageCreateInfo.usage = usage;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkCreateImage(context.device, &imageCreateInfo, nullptr, &texture->image);

  //Allocate and bind memory for the image.
  //note: Memory for the image is not host visible so we will need a host visible buffer to transfer the data
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device, texture->image, &requirements);
  texture->memory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context.device, texture->image, texture->memory.handle, 0);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.format = imageCreateInfo.format;
  imageViewCreateInfo.image = texture->image;
  imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &texture->imageView);

  //Create sampler
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)sampler.magnification;
  samplerCreateInfo.minFilter = (VkFilter)sampler.minification;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)sampler.mipmap;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)sampler.wrapU;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)sampler.wrapV;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)sampler.wrapW;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = (float)mipLevels;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device, &samplerCreateInfo, nullptr, &texture->sampler);

  texture->descriptor = {};
  texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->descriptor.imageView = texture->imageView;
  texture->descriptor.sampler = texture->sampler;
  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->mipLevels = 1;
  texture->aspectFlags = aspectFlags;
  texture->format = format;
  texture->extent = extents;
}

void render::textureCubemapCreate(const context_t& context, VkFormat format, uint32_t width, uint32_t height, uint32_t mipLevels, texture_sampler_t sampler, texture_t* texture)
{
  //Get base level image width and height
  VkExtent3D extents = { width, height, 1u };

  //Create the image
  VkImageCreateInfo imageCreateInfo = {};
  imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageCreateInfo.pNext = nullptr;
  imageCreateInfo.mipLevels = mipLevels;
  imageCreateInfo.format = format;
  imageCreateInfo.arrayLayers = 6;  //Cubemap faces
  imageCreateInfo.extent = extents;
  imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  vkCreateImage(context.device, &imageCreateInfo, nullptr, &texture->image);


  //Allocate and bind memory for the image.
  //note: Memory for the image is not host visible so we will need a host visible buffer to transfer the data
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device, texture->image, &requirements);
  texture->memory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context.device, texture->image, texture->memory.handle, texture->memory.offset);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.format = imageCreateInfo.format;
  imageViewCreateInfo.image = texture->image;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.levelCount = mipLevels;
  imageViewCreateInfo.subresourceRange.layerCount = 6;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &texture->imageView);

  //Create sampler
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)sampler.magnification;
  samplerCreateInfo.minFilter = (VkFilter)sampler.minification;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)sampler.mipmap;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)sampler.wrapU;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)sampler.wrapV;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)sampler.wrapW;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = (float)mipLevels;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device, &samplerCreateInfo, nullptr, &texture->sampler);


  texture->descriptor = {};
  texture->descriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->descriptor.imageView = texture->imageView;
  texture->descriptor.sampler = texture->sampler;
  texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->extent = extents;
  texture->mipLevels = mipLevels;
  texture->aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  texture->format = format;
}

void render::textureCubemapCreate(const context_t& context, const image::image2D_t* images, uint32_t mipLevels, texture_sampler_t sampler, texture_t* texture)
{
  VkExtent3D extents = { images[0].width, images[0].height, 1u };
  VkFormat format = getImageFormat(images[0]);
  textureCubemapCreate(context, format, images[0].width, images[0].height, mipLevels, sampler, texture);
  
  //Create a staging buffer and memory store for the buffer  
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device, texture->image, &requirements);
  VkBuffer stagingBuffer;
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.pNext = nullptr;
  bufferCreateInfo.size = requirements.size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(context.device, &bufferCreateInfo, nullptr, &stagingBuffer);

  //Allocate and bind memory to the buffer
  vkGetBufferMemoryRequirements(context.device, stagingBuffer, &requirements);
  gpu_memory_t stagingBufferMemory;
  stagingBufferMemory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, HOST_VISIBLE);
  vkBindBufferMemory(context.device, stagingBuffer, stagingBufferMemory.handle, stagingBufferMemory.offset);

  //@TODO: Handle mipmaps
  //Map buffer memory and copy image data
  unsigned char* mapping = (unsigned char*)gpuMemoryMap(context, stagingBufferMemory);
  if (mapping)
  {
    for (uint32_t i(0); i < 6; ++i)
    {
      memcpy(mapping, images[i].data, images[i].dataSize);
      mapping += images[i].dataSize;
    }

    gpuMemoryUnmap(context, stagingBufferMemory);
  }

  std::vector<VkBufferImageCopy> bufferCopyRegions;
  size_t offset = 0;
  for (uint32_t face = 0; face < 6; face++)
  {
    for (uint32_t level = 0; level < mipLevels; level++)
    {
      VkBufferImageCopy bufferCopyRegion = {};
      bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      bufferCopyRegion.imageSubresource.mipLevel = level;
      bufferCopyRegion.imageSubresource.baseArrayLayer = face;
      bufferCopyRegion.imageSubresource.layerCount = 1;
      bufferCopyRegion.imageExtent.width = extents.width;
      bufferCopyRegion.imageExtent.height = extents.height;
      bufferCopyRegion.imageExtent.depth = 1;
      bufferCopyRegion.bufferOffset = offset;

      bufferCopyRegions.push_back(bufferCopyRegion);

      offset += images[0].dataSize;
    }
  }

  //Create command buffer
  VkCommandBuffer uploadCommandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &uploadCommandBuffer);

  //Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(uploadCommandBuffer, &beginInfo);

  ////Copy data from the buffer to the image
  //Transition image layout from undefined to optimal-for-transfer-destination
  VkImageMemoryBarrier imageBarrier = {};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageBarrier.pNext = nullptr;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.srcAccessMask = 0;
  imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageBarrier.image = texture->image;
  imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageBarrier.subresourceRange.layerCount = 1;
  imageBarrier.subresourceRange.levelCount = 1;

  vkCmdPipelineBarrier(uploadCommandBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);

  //Copy from buffer to image
  //@ToDO : Copy mipmaps. Currently only base level is copied to the image
  vkCmdCopyBufferToImage(uploadCommandBuffer, stagingBuffer,
    texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    6, bufferCopyRegions.data() );

  //Transition image layout from optimal-for-transfer to optimal-for-shader-reads
  imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

  vkCmdPipelineBarrier(uploadCommandBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);

  //End command buffer
  vkEndCommandBuffer(uploadCommandBuffer);

  ////Queue uploadCommandBuffer for execution
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &uploadCommandBuffer;

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkFence fence;
  vkCreateFence(context.device, &fenceCreateInfo, nullptr, &fence);
  vkResetFences(context.device, 1u, &fence);
  vkQueueSubmit(context.graphicsQueue.handle, 1, &submitInfo, fence);

  //Destroy temporary resources
  vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(context.device, fence, nullptr);
  vkFreeCommandBuffers(context.device, context.commandPool, 1, &uploadCommandBuffer);
  gpuMemoryDeallocate(context, nullptr, stagingBufferMemory);
  vkDestroyBuffer(context.device, stagingBuffer, nullptr);

  textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture);
}


void render::textureDestroy(const context_t& context, texture_t* texture)
{
  vkDestroyImageView(context.device, texture->imageView, nullptr);
  vkDestroyImage(context.device, texture->image, nullptr);
  vkDestroySampler(context.device, texture->sampler, nullptr);
  gpuMemoryDeallocate(context, nullptr, texture->memory);
}

void render::textureCopy(const command_buffer_t& commandBuffer, texture_t* srcTexture, texture_t* dstTexture,
                         uint32_t width, uint32_t height, uint32_t dstMipmap, uint32_t dstLayer, uint32_t srcMipmap, uint32_t srcLayer)
{  
  VkImageCopy copyRegion = {};
  copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.srcSubresource.baseArrayLayer = srcLayer;
  copyRegion.srcSubresource.mipLevel = srcMipmap;
  copyRegion.srcSubresource.layerCount = 1;
  copyRegion.srcOffset = { 0, 0, 0 };

  copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copyRegion.dstSubresource.baseArrayLayer = dstLayer;
  copyRegion.dstSubresource.mipLevel = dstMipmap;
  copyRegion.dstSubresource.layerCount = 1;
  copyRegion.dstOffset = { 0, 0, 0 };

  copyRegion.extent.width = width;
  copyRegion.extent.height = height;
  copyRegion.extent.depth = 1;

  vkCmdCopyImage(commandBuffer.handle,
    srcTexture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    dstTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1, &copyRegion);
}

void render::textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subResourceRange, texture_t* texture)
{
  VkImageMemoryBarrier imageMemoryBarrier = {};
  imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageMemoryBarrier.pNext = nullptr;
  imageMemoryBarrier.oldLayout = texture->layout;
  imageMemoryBarrier.newLayout = newLayout;
  imageMemoryBarrier.image = texture->image;
  imageMemoryBarrier.subresourceRange = subResourceRange;

  // Source layouts (old)
  // Source access mask controls actions that have to be finished on the old layout
  // before it will be transitioned to the new layout
  switch (texture->layout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    // Image layout is undefined (or does not matter)
    // Only valid as initial layout
    // No flags required, listed only for completeness
    imageMemoryBarrier.srcAccessMask = 0;
    break;

  case VK_IMAGE_LAYOUT_PREINITIALIZED:
    // Image is preinitialized
    // Only valid as initial layout for linear images, preserves memory contents
    // Make sure host writes have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    // Image is a color attachment
    // Make sure any writes to the color buffer have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    // Image is a depth/stencil attachment
    // Make sure any writes to the depth/stencil buffer have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    // Image is a transfer source 
    // Make sure any reads from the image have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    // Image is a transfer destination
    // Make sure any writes to the image have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    // Image is read by a shader
    // Make sure any shader reads from the image have been finished
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    break;
  default:
    // Other source layouts aren't handled (yet)
    break;
  }

  // Target layouts (new)
  // Destination access mask controls the dependency for the new image layout
  switch (newLayout)
  {
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    // Image will be used as a transfer destination
    // Make sure any writes to the image have been finished
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    // Image will be used as a transfer source
    // Make sure any reads from the image have been finished
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    // Image will be used as a color attachment
    // Make sure any writes to the color buffer have been finished
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    // Image layout will be used as a depth/stencil attachment
    // Make sure any writes to depth/stencil buffer have been finished
    imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    // Image will be read in a shader (sampler, input attachment)
    // Make sure any writes to the image have been finished
    if (imageMemoryBarrier.srcAccessMask == 0)
    {
      imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    break;
  default:
    // Other source layouts aren't handled (yet)
    break;
  }

  // Put barrier inside setup command buffer
  vkCmdPipelineBarrier(
    cmdBuffer.handle,
    srcStageMask,
    dstStageMask,
    0,
    0, nullptr,
    0, nullptr,
    1, &imageMemoryBarrier);


  texture->layout = newLayout;
  texture->descriptor.imageLayout = newLayout;
}

void render::textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, texture_t* texture)
{
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.levelCount = 1u;
  subresourceRange.layerCount = 1u;
  subresourceRange.aspectMask = texture->aspectFlags;
  textureChangeLayout(cmdBuffer, layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, texture);
}

void render::textureChangeLayout(command_buffer_t cmdBuffer, VkImageLayout layout, texture_t* texture)
{
  textureChangeLayout(cmdBuffer, layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, texture);
}

void render::textureChangeLayoutNow(const context_t& context, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageSubresourceRange subResourceRange, texture_t* texture)
{
  if (layout == texture->layout)
  {
    return;
  }

  //Create command buffer
  command_buffer_t commandBuffer;
  commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, 
    nullptr, 0u, command_buffer_t::GRAPHICS, &commandBuffer);
    
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer.handle, &beginInfo);
  textureChangeLayout(commandBuffer, layout, srcStageMask, dstStageMask, subResourceRange, texture);
  vkEndCommandBuffer(commandBuffer.handle);

  ////Queue commandBuffer for execution
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer.handle;
  
  vkResetFences(context.device, 1u, &commandBuffer.fence);
  vkQueueSubmit(context.graphicsQueue.handle, 1, &submitInfo, commandBuffer.fence);

  //Destroy command buffer
  vkWaitForFences(context.device, 1u, &commandBuffer.fence, VK_TRUE, UINT64_MAX);
  render::commandBufferDestroy(context, &commandBuffer);
}

void render::textureChangeLayoutNow(const context_t& context, VkImageLayout layout, texture_t* texture)
{
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.levelCount = 1u;
  subresourceRange.layerCount = 1u;
  subresourceRange.aspectMask = texture->aspectFlags;
  textureChangeLayoutNow(context, layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, texture);
}


void render::gpuBufferCreate(const context_t& context,
  uint32_t usage, uint32_t memoryType, void* data,
  size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer)
{
  //Create the buffer
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = static_cast<uint32_t> (size);
  bufferCreateInfo.usage = (VkBufferUsageFlagBits)usage;
  vkCreateBuffer(context.device, &bufferCreateInfo, nullptr, &buffer->handle);

  //Get memory requirements of the buffer
  VkMemoryRequirements requirements = {};
  vkGetBufferMemoryRequirements(context.device, buffer->handle, &requirements);

  //Allocate memory for the buffer
  buffer->memory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, 0xFFFF, memoryType, allocator);

  //Bind memory to the buffer
  vkBindBufferMemory(context.device, buffer->handle, buffer->memory.handle, buffer->memory.offset);

  if (data)
  {
    //Fill the buffer
    void* mapping = gpuMemoryMap(context, buffer->memory);
    assert(mapping);
    memcpy(mapping, data, size);
    gpuMemoryUnmap(context, buffer->memory);
  }

  //Initialize descriptor
  buffer->descriptor = {};
  buffer->descriptor.offset = 0;
  buffer->descriptor.buffer = buffer->handle;
  buffer->descriptor.range = size;

  buffer->usage = usage;
}

void render::gpuBufferCreate(const context_t& context, uint32_t usage, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer)
{
  return gpuBufferCreate(context, usage, HOST_VISIBLE, data, size, allocator, buffer);
}

void render::gpuBufferDestroy(const context_t& context, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer)
{
  vkDestroyBuffer(context.device, buffer->handle, nullptr);
  gpuMemoryDeallocate(context, allocator, buffer->memory);
}

void render::gpuBufferUpdate(const context_t& context, void* data, size_t offset, size_t size, gpu_buffer_t* buffer)
{
  void* mapping = gpuMemoryMap(context, offset, size, 0u, buffer->memory);
  assert(mapping);
  memcpy(mapping, data, size);
  gpuMemoryUnmap(context, buffer->memory);
}

void* render::gpuBufferMap(const context_t& context, const gpu_buffer_t& buffer)
{
  return gpuMemoryMap(context, buffer.memory.offset, buffer.memory.size, 0u, buffer.memory);
}

void render::gpuBufferUnmap(const context_t& context, const gpu_buffer_t& buffer)
{
  gpuMemoryUnmap(context, buffer.memory);
}

descriptor_t render::getDescriptor(const gpu_buffer_t& buffer)
{
  descriptor_t descriptor;
  descriptor.bufferDescriptor = buffer.descriptor;
  return descriptor;
}

descriptor_t render::getDescriptor(const texture_t& texture)
{
  descriptor_t descriptor;
  descriptor.imageDescriptor = texture.descriptor;
  return descriptor;
}

void render::descriptorSetLayoutCreate(const context_t& context, descriptor_binding_t* bindings, uint32_t bindingCount, descriptor_set_layout_t* descriptorSetLayout)
{
  descriptorSetLayout->bindingCount = bindingCount;
  descriptorSetLayout->bindings = nullptr;
  descriptorSetLayout->handle = VK_NULL_HANDLE;

  if (bindingCount > 0)
  {
    descriptorSetLayout->bindings = new descriptor_binding_t[bindingCount];
    memcpy(descriptorSetLayout->bindings, bindings, sizeof(descriptor_binding_t)*bindingCount);
  }
  
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings(bindingCount);
  for (uint32_t i(0); i < layoutBindings.size(); ++i)
  {
    layoutBindings[i].descriptorCount = 1;
    layoutBindings[i].descriptorType = (VkDescriptorType)descriptorSetLayout->bindings[i].type;
    layoutBindings[i].binding = descriptorSetLayout->bindings[i].binding;
    layoutBindings[i].stageFlags = descriptorSetLayout->bindings[i].stageFlags;
  }

  //Create DescriptorSetLayout
  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
  descriptorSetLayoutCreateInfo.pNext = NULL;
  descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  if (!layoutBindings.empty())
  {
    descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)layoutBindings.size();
    descriptorSetLayoutCreateInfo.pBindings = &layoutBindings[0];
  }

  vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout->handle);  
}

void render::descriptorSetLayoutDestroy(const context_t& context, descriptor_set_layout_t* desriptorSetLayout)
{
  delete[] desriptorSetLayout->bindings;
  vkDestroyDescriptorSetLayout(context.device, desriptorSetLayout->handle, nullptr);
}

void render::pipelineLayoutCreate(const context_t& context,
                                  descriptor_set_layout_t* descriptorSetLayouts, uint32_t descriptorSetLayoutCount,
                                  push_constant_range_t* pushConstantRanges, uint32_t pushConstantRangeCount,
                                  pipeline_layout_t* pipelineLayout)
{
  //Create pipeline layout
  pipelineLayout->handle = VK_NULL_HANDLE;


  pipelineLayout->descriptorSetLayout = nullptr;
  pipelineLayout->descriptorSetLayoutCount = descriptorSetLayoutCount;
  std::vector<VkDescriptorSetLayout> setLayouts(descriptorSetLayoutCount);
  if (descriptorSetLayoutCount > 0)
  {
    for (uint32_t i(0); i < descriptorSetLayoutCount; ++i)
    {
      setLayouts[i] = descriptorSetLayouts[i].handle;
    }

    pipelineLayout->descriptorSetLayout = new descriptor_set_layout_t[descriptorSetLayoutCount];
    memcpy(pipelineLayout->descriptorSetLayout, descriptorSetLayouts, sizeof(descriptor_set_layout_t)*descriptorSetLayoutCount);
  }

  pipelineLayout->pushConstantRange = nullptr;
  pipelineLayout->pushConstantRangeCount = pushConstantRangeCount;
  std::vector<VkPushConstantRange> pushConstants(pushConstantRangeCount);
  if (pushConstantRangeCount > 0)
  {
    VkPushConstantRange pushConstantRange;
    for (uint32_t i(0); i < pushConstantRangeCount; ++i)
    { 
      pushConstantRange.stageFlags = pushConstantRanges[i].stageFlags;
      pushConstantRange.offset = pushConstantRanges[i].offset;
      pushConstantRange.size = pushConstantRanges[i].size;
      pushConstants[i] = pushConstantRange;
    }

    pipelineLayout->pushConstantRange = new push_constant_range_t[pushConstantRangeCount];
    memcpy(pipelineLayout->pushConstantRange, pushConstantRanges, sizeof(push_constant_range_t)*pushConstantRangeCount );
  }


  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutCount;
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
  pipelineLayoutCreateInfo.pPushConstantRanges = pushConstants.data();
  pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRangeCount;
  vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout->handle);
}

void render::pipelineLayoutDestroy(const context_t& context, pipeline_layout_t* pipelineLayout)
{
  delete[] pipelineLayout->descriptorSetLayout;
  delete[] pipelineLayout->pushConstantRange;
  vkDestroyPipelineLayout(context.device, pipelineLayout->handle, nullptr);
}

void render::descriptorPoolCreate(const context_t& context, uint32_t descriptorSetsCount,
  combined_image_sampler_count combinedImageSamplers, uniform_buffer_count uniformBuffers,
  storage_buffer_count storageBuffers, storage_image_count storageImages,
  descriptor_pool_t* descriptorPool)
{
  descriptorPool->descriptorSets = descriptorSetsCount;
  descriptorPool->combinedImageSamplers = combinedImageSamplers.data;
  descriptorPool->uniformBuffers = uniformBuffers.data;
  descriptorPool->storageBuffers = storageBuffers.data;
  descriptorPool->storageImages = storageImages.data;

  std::vector<VkDescriptorPoolSize> descriptorPoolSize;

  if(descriptorPool->combinedImageSamplers > 0u )
    descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorPool->combinedImageSamplers });

  if (descriptorPool->uniformBuffers > 0u)
    descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorPool->uniformBuffers });

  if (descriptorPool->storageBuffers > 0u)
    descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorPool->storageBuffers });

  if (descriptorPool->storageImages > 0u)
    descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorPool->storageImages });

  //Create DescriptorPool
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolCreateInfo.maxSets = descriptorPool->descriptorSets;
  descriptorPoolCreateInfo.poolSizeCount = (uint32_t)descriptorPoolSize.size();
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize.data();

  vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, nullptr, &descriptorPool->handle);
}

void render::descriptorPoolDestroy(const context_t& context, descriptor_pool_t* descriptorPool)
{
  vkDestroyDescriptorPool(context.device, descriptorPool->handle, nullptr);
}

void render::descriptorSetCreate(const context_t& context, const descriptor_pool_t& descriptorPool, const descriptor_set_layout_t& descriptorSetLayout, descriptor_t* descriptors, descriptor_set_t* descriptorSet)
{
  descriptorSet->descriptorCount = descriptorSetLayout.bindingCount;
  descriptorSet->descriptors = new descriptor_t[descriptorSet->descriptorCount];
  memcpy(descriptorSet->descriptors, descriptors, sizeof(descriptor_t)*descriptorSet->descriptorCount);
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout.handle;
  descriptorSetAllocateInfo.descriptorSetCount = 1;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool.handle;
  vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo, &descriptorSet->handle);
  descriptorSetUpdate(context, descriptorSetLayout, descriptorSet);

  descriptorSet->pool = descriptorPool;
}

void render::descriptorSetDestroy(const context_t& context, descriptor_set_t* descriptorSet)
{
  delete[] descriptorSet->descriptors;
  vkFreeDescriptorSets(context.device, descriptorSet->pool.handle, 1, &descriptorSet->handle);
}

void render::descriptorSetUpdate(const context_t& context, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet)
{
  std::vector<VkWriteDescriptorSet> writeDescriptorSets(descriptorSet->descriptorCount);
  for (uint32_t i(0); i < writeDescriptorSets.size(); ++i)
  {
    writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[i].dstSet = descriptorSet->handle;
    writeDescriptorSets[i].descriptorCount = 1;
    writeDescriptorSets[i].descriptorType = (VkDescriptorType)descriptorSetLayout.bindings[i].type;
    writeDescriptorSets[i].dstBinding = descriptorSetLayout.bindings[i].binding;

    switch (descriptorSetLayout.bindings[i].type)
    {
      case descriptor_t::type_e::SAMPLER:
      case descriptor_t::type_e::COMBINED_IMAGE_SAMPLER:
      case descriptor_t::type_e::SAMPLED_IMAGE:
      case descriptor_t::type_e::STORAGE_IMAGE:
      {
        writeDescriptorSets[i].pImageInfo = &descriptorSet->descriptors[i].imageDescriptor;
        break;
      }

      case descriptor_t::type_e::UNIFORM_TEXEL_BUFFER:
      case descriptor_t::type_e::STORAGE_TEXEL_BUFFER:
      case descriptor_t::type_e::UNIFORM_BUFFER:
      case descriptor_t::type_e::STORAGE_BUFFER:
      case descriptor_t::type_e::UNIFORM_BUFFER_DYNAMIC:
      case descriptor_t::type_e::STORAGE_BUFFER_DYNAMIC:
      case descriptor_t::type_e::INPUT_ATTACHMENT:
      {
        writeDescriptorSets[i].pBufferInfo = &descriptorSet->descriptors[i].bufferDescriptor;
        break;
      }
    }
  }

  vkUpdateDescriptorSets(context.device, (uint32_t)writeDescriptorSets.size(), &writeDescriptorSets[0], 0, nullptr);
}

void render::descriptorSetBind(command_buffer_t commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount)
{
  VkPipelineBindPoint bindPoint = commandBuffer.type == command_buffer_t::GRAPHICS ? VK_PIPELINE_BIND_POINT_GRAPHICS :
                                                                                     VK_PIPELINE_BIND_POINT_COMPUTE;
  
  std::vector<VkDescriptorSet> descriptorSetHandles(descriptorSetCount);
  for (u32 i(0); i < descriptorSetCount; ++i)
  {
    descriptorSetHandles[i] = descriptorSets[i].handle;
  }
  
  vkCmdBindDescriptorSets(commandBuffer.handle, bindPoint, pipelineLayout.handle, firstSet, descriptorSetCount, descriptorSetHandles.data(), 0, 0);
}

void render::graphicsPipelineCreate(const context_t& context, VkRenderPass renderPass, uint32_t subpass, const render::vertex_format_t& vertexFormat, 
  const pipeline_layout_t& pipelineLayout, const graphics_pipeline_t::description_t& pipelineDesc, graphics_pipeline_t* pipeline)
{
  pipeline->desc = pipelineDesc;
  VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
  pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  pipelineViewportStateCreateInfo.viewportCount = 1;
  pipelineViewportStateCreateInfo.pViewports = &pipeline->desc.viewPort;
  pipelineViewportStateCreateInfo.scissorCount = 1;
  pipelineViewportStateCreateInfo.pScissors = &pipeline->desc.scissorRect;

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
  pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  pipelineColorBlendStateCreateInfo.attachmentCount = (uint32_t)pipeline->desc.blendState.size();
  pipelineColorBlendStateCreateInfo.pAttachments = &pipeline->desc.blendState[0];

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
  pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
  pipelineRasterizationStateCreateInfo.cullMode = pipeline->desc.cullMode;
  pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
  pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  pipelineDepthStencilStateCreateInfo.depthTestEnable = pipeline->desc.depthTestEnabled;
  pipelineDepthStencilStateCreateInfo.depthWriteEnable = pipeline->desc.depthWriteEnabled;
  pipelineDepthStencilStateCreateInfo.depthCompareOp = pipeline->desc.depthTestFunction;
  pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
  pipelineDepthStencilStateCreateInfo.back.failOp = VK_STENCIL_OP_KEEP;
  pipelineDepthStencilStateCreateInfo.back.passOp = VK_STENCIL_OP_KEEP;
  pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
  pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
  pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;

  VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
  pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[2] = {};
  pipelineShaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineShaderStageCreateInfos[0].module = pipeline->desc.vertexShader.handle;
  pipelineShaderStageCreateInfos[0].pName = "main";
  pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

  pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineShaderStageCreateInfos[1].module = pipeline->desc.fragmentShader.handle;
  pipelineShaderStageCreateInfos[1].pName = "main";
  pipelineShaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
  graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

  //Make viewport and scissor context dynamic (can be changed by command buffer)
  VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT,	VK_DYNAMIC_STATE_SCISSOR };
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.pDynamicStates = dynamicStateEnables;
  dynamicState.dynamicStateCount = 2;

  graphicsPipelineCreateInfo.layout = pipelineLayout.handle;
  graphicsPipelineCreateInfo.pVertexInputState = &vertexFormat.vertexInputState;
  graphicsPipelineCreateInfo.pInputAssemblyState = &vertexFormat.inputAssemblyState;
  graphicsPipelineCreateInfo.renderPass = renderPass;
  graphicsPipelineCreateInfo.subpass = subpass;
  graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
  graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
  graphicsPipelineCreateInfo.pDynamicState = &dynamicState;
  graphicsPipelineCreateInfo.stageCount = 2;
  vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline->handle);

  pipeline->layout = pipelineLayout;
}

void render::graphicsPipelineDestroy(const context_t& context, graphics_pipeline_t* pipeline)
{
  vkDestroyPipeline(context.device, pipeline->handle, nullptr);  
}

void render::graphicsPipelineBind(command_buffer_t commandBuffer, const graphics_pipeline_t& pipeline)
{
  vkCmdBindPipeline(commandBuffer.handle, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
}


void render::computePipelineCreate(const context_t& context, const pipeline_layout_t& layout, const render::shader_t& computeShader, compute_pipeline_t* pipeline)
{
  //Compute pipeline
  pipeline->computeShader = computeShader;

  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.pName = "main";
  shaderStage.module = pipeline->computeShader.handle;

  VkComputePipelineCreateInfo computePipelineCreateInfo = {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = layout.handle;
  computePipelineCreateInfo.flags = 0;
  computePipelineCreateInfo.stage = shaderStage;
  vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline->handle);
}

void render::computePipelineDestroy(const context_t& context, compute_pipeline_t* pipeline)
{
  vkDestroyPipeline(context.device, pipeline->handle, nullptr);
}

void render::computePipelineBind(command_buffer_t commandBuffer, const compute_pipeline_t& pipeline)
{
  vkCmdBindPipeline(commandBuffer.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.handle);
}

void render::computeDispatch(command_buffer_t commandBuffer, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ)
{
  vkCmdDispatch(commandBuffer.handle, groupSizeX, groupSizeY, groupSizeZ);
}

void render::pushConstants(command_buffer_t commandBuffer, pipeline_layout_t pipelineLayout, uint32_t offset, const void* constant)
{
  for (uint32_t i(0); i < pipelineLayout.pushConstantRangeCount; ++i)
  {
    if (pipelineLayout.pushConstantRange[i].offset == offset)
    {
      vkCmdPushConstants(commandBuffer.handle, pipelineLayout.handle, pipelineLayout.pushConstantRange[i].stageFlags, offset, pipelineLayout.pushConstantRange[i].size, constant);
      break;
    }
  }
}

void render::setViewport(const command_buffer_t& commandBuffer, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
  VkViewport viewPort = { (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f };
  vkCmdSetViewport(commandBuffer.handle, 0, 1, &viewPort);
}
void render::setScissor(const command_buffer_t& commandBuffer, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
  VkRect2D scissorRect = { { x,y },{ width,height } };
  vkCmdSetScissor(commandBuffer.handle, 0, 1, &scissorRect);
}

static const VkFormat AttributeFormatLUT[] = { VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32_SFLOAT,
                                               VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_SFLOAT,
                                               VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT,
                                               VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SFLOAT,
                                               VK_FORMAT_R8G8B8A8_UNORM
                                             };

static const uint32_t AttributeFormatSizeLUT[] = { 4u, 4u, 4u, 
                                                   8u, 8u, 8u, 
                                                   12u, 12u, 12u, 
                                                   16u, 16u, 16u,
                                                   4u
                                                 };

void render::vertexFormatCreate(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format)
{
  format->vertexSize = 0u;

  format->attributes = new vertex_attribute_t[attributeCount];
  memcpy(format->attributes, attribute, sizeof(vertex_attribute_t)*attributeCount);

  VkVertexInputAttributeDescription* attributeDescription = new VkVertexInputAttributeDescription[attributeCount];
  VkVertexInputBindingDescription* bindingDescription = new VkVertexInputBindingDescription[attributeCount];
  for (uint32_t i = 0; i < attributeCount; ++i)
  {
    VkFormat attributeFormat = AttributeFormatLUT[attribute[i].format];
    bindingDescription[i].binding = i;
    bindingDescription[i].inputRate = attribute[i].instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescription[i].stride = (uint32_t)attribute[i].stride;
    attributeDescription[i].binding = i;
    attributeDescription[i].format = attributeFormat;
    attributeDescription[i].location = i;
    attributeDescription[i].offset = attribute[i].offset;
    format->vertexSize += AttributeFormatSizeLUT[attribute[i].format];
  }

  format->vertexInputState = {};
  format->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  format->vertexInputState.vertexAttributeDescriptionCount = attributeCount;
  format->vertexInputState.pVertexAttributeDescriptions = attributeDescription;
  format->vertexInputState.vertexBindingDescriptionCount = attributeCount;
  format->vertexInputState.pVertexBindingDescriptions = bindingDescription;

  format->inputAssemblyState = {};
  format->inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  format->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  format->attributeCount = attributeCount;
}

void render::vertexFormatCopy(const vertex_format_t* formatSrc, vertex_format_t* formatDst)
{
  vertexFormatCreate(formatSrc->attributes, formatSrc->attributeCount, formatDst);
};

void render::vertexFormatAddAttributes(vertex_attribute_t* newAttribute, uint32_t newattributeCount, vertex_format_t* format)
{ 
  u32 oldAttributeCount = format->attributeCount;
  u32 attributeCount = newattributeCount + oldAttributeCount;

  vertex_attribute_t* oldAttributes = format->attributes;
  format->vertexSize = 0u;

  format->attributes = new vertex_attribute_t[attributeCount];
  memcpy(format->attributes, oldAttributes, sizeof(vertex_attribute_t)*oldAttributeCount);
  memcpy(format->attributes + oldAttributeCount, newAttribute, sizeof(vertex_attribute_t)*newattributeCount);

  delete[] format->vertexInputState.pVertexAttributeDescriptions;
  delete[] format->vertexInputState.pVertexBindingDescriptions;
  VkVertexInputAttributeDescription* attributeDescription = new VkVertexInputAttributeDescription[attributeCount];
  VkVertexInputBindingDescription* bindingDescription = new VkVertexInputBindingDescription[attributeCount];

  //Old attributes
  uint32_t i = 0;
  for (; i < oldAttributeCount; ++i)
  {
    VkFormat attributeFormat = AttributeFormatLUT[oldAttributes[i].format];
    bindingDescription[i].binding = i;
    bindingDescription[i].inputRate = oldAttributes[i].instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescription[i].stride = (uint32_t)oldAttributes[i].stride;
    attributeDescription[i].binding = i;
    attributeDescription[i].format = attributeFormat;
    attributeDescription[i].location = i;
    attributeDescription[i].offset = oldAttributes[i].offset;
    format->vertexSize += AttributeFormatSizeLUT[oldAttributes[i].format];
  }

  //New attributes
  for (u32 j = 0; i < attributeCount; ++i, ++j)
  {
    VkFormat attributeFormat = AttributeFormatLUT[newAttribute[j].format];
    bindingDescription[i].binding = i;
    bindingDescription[i].inputRate = newAttribute[j].instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescription[i].stride = (uint32_t)newAttribute[j].stride;
    attributeDescription[i].binding = i;
    attributeDescription[i].format = attributeFormat;
    attributeDescription[i].location = i;
    attributeDescription[i].offset = newAttribute[j].offset;
    format->vertexSize += AttributeFormatSizeLUT[newAttribute[j].format];
  }

  format->vertexInputState = {};
  format->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  format->vertexInputState.vertexAttributeDescriptionCount = attributeCount;
  format->vertexInputState.pVertexAttributeDescriptions = attributeDescription;
  format->vertexInputState.vertexBindingDescriptionCount = attributeCount;
  format->vertexInputState.pVertexBindingDescriptions = bindingDescription;

  format->inputAssemblyState = {};
  format->inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  format->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  format->attributeCount = attributeCount;
  delete[] oldAttributes;
}

void render::vertexFormatDestroy(vertex_format_t* format)
{
  delete[] format->vertexInputState.pVertexAttributeDescriptions;
  delete[] format->vertexInputState.pVertexBindingDescriptions;
  delete[] format->attributes;
}

void render::depthStencilBufferCreate(const context_t& context, uint32_t width, uint32_t height, depth_stencil_buffer_t* depthStencilBuffer)
{
  CreateDepthStencilBuffer(&context, width, height, context.swapChain.depthStencil.format, depthStencilBuffer);
  
  
  //Create sampler
  texture_sampler_t defaultSampler;
  VkSampler sampler;
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)defaultSampler.magnification;
  samplerCreateInfo.minFilter = (VkFilter)defaultSampler.minification;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)defaultSampler.mipmap;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)defaultSampler.wrapU;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)defaultSampler.wrapV;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)defaultSampler.wrapW;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = 0.0;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device, &samplerCreateInfo, nullptr, &sampler);

  depthStencilBuffer->descriptor.sampler = sampler;
  depthStencilBuffer->descriptor.imageView = depthStencilBuffer->imageView;
  depthStencilBuffer->descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depthStencilBuffer->aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  depthStencilBuffer->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void render::depthStencilBufferDestroy(const context_t& context, depth_stencil_buffer_t* depthStencilBuffer)
{
  vkDestroyImageView(context.device, depthStencilBuffer->imageView, nullptr);
  vkDestroyImage(context.device, depthStencilBuffer->image, nullptr);
  vkDestroySampler(context.device, depthStencilBuffer->descriptor.sampler, nullptr);
  gpuMemoryDeallocate(context, nullptr, depthStencilBuffer->memory);
}

void render::renderPassCreate(const context_t& context,
  render_pass_t::attachment_t* attachments, uint32_t attachmentCount,
  render_pass_t::subpass_t* subpasses, uint32_t subpassCount,
  render_pass_t::subpass_dependency_t* dependencies, uint32_t dependencyCount,
  render_pass_t* renderPass)
{
  renderPass->attachment = new render_pass_t::attachment_t[attachmentCount];
  renderPass->attachmentCount = attachmentCount;
  memcpy(renderPass->attachment, attachments, sizeof(render_pass_t::attachment_t)*attachmentCount);

  std::vector<VkAttachmentDescription> attachmentDescription(attachmentCount);
  for (uint32_t i(0); i < attachmentCount; ++i)
  {
    attachmentDescription[i].samples = attachments[i].samples;
    attachmentDescription[i].format = attachments[i].format;
    attachmentDescription[i].loadOp = attachments[i].loadOp;
    attachmentDescription[i].storeOp = attachments[i].storeOp;
    attachmentDescription[i].initialLayout = attachments[i].initialLayout;
    attachmentDescription[i].finalLayout = attachments[i].finalLayout;
  }

  
  if (subpassCount == 0u)
  {
    //Create default subpass
    VkSubpassDescription subpassDescription = {};
    std::vector<VkAttachmentReference> attachmentColorReference;
    VkAttachmentReference depthStencilAttachmentReference;
    for (u32 i(0); i < attachmentDescription.size(); ++i)
    {
      if (attachmentDescription[i].format == context.swapChain.depthStencil.format)
      {
        depthStencilAttachmentReference.attachment = i;
        depthStencilAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpassDescription.pDepthStencilAttachment = &depthStencilAttachmentReference;
      }
      else
      {
        VkAttachmentReference attachmentRef = {};
        attachmentRef.attachment = i;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentColorReference.push_back(attachmentRef);
      }
    }
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pColorAttachments = attachmentColorReference.data();
    subpassDescription.colorAttachmentCount = (uint32_t)attachmentColorReference.size();

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = attachmentCount;
    renderPassCreateInfo.subpassCount = 1u;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.pAttachments = attachmentDescription.data();


    vkCreateRenderPass(context.device, &renderPassCreateInfo, nullptr, &renderPass->handle);
  }
  else
  {
    //Create subpasses descriptions
    std::vector<VkSubpassDescription> subpassDescription(subpassCount);
    std::vector< std::vector<VkAttachmentReference> > inputAttachmentRef( subpassCount );
    std::vector< std::vector<VkAttachmentReference> > colorAttachmentRef(subpassCount);
    std::vector<VkAttachmentReference> depthStencilAttachmentRef(subpassCount);
    
    for (uint32_t i = 0; i < subpassCount; ++i)
    {
      subpassDescription[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

      //Input attachments
      uint32_t inputAttachmentCount = (uint32_t)subpasses[i].inputAttachmentIndex.size();
      inputAttachmentRef[i].resize(inputAttachmentCount);
      for (uint32_t j = 0; j < inputAttachmentCount; ++j)
      {
        inputAttachmentRef[i][j].attachment = subpasses[i].inputAttachmentIndex[j];
        inputAttachmentRef[i][j].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
      subpassDescription[i].inputAttachmentCount = inputAttachmentCount;
      subpassDescription[i].pInputAttachments = inputAttachmentRef[i].data();

      //Color attachments
      uint32_t colorAttachmentCount = (uint32_t)subpasses[i].colorAttachmentIndex.size();
      colorAttachmentRef[i].resize(colorAttachmentCount);
      for (uint32_t j = 0; j < colorAttachmentCount; ++j)
      {
        colorAttachmentRef[i][j].attachment = subpasses[i].colorAttachmentIndex[j];
        colorAttachmentRef[i][j].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      subpassDescription[i].colorAttachmentCount = colorAttachmentCount;
      subpassDescription[i].pColorAttachments = colorAttachmentRef[i].data();
      

      //Depth stencil attachment
      if (subpasses[i].depthStencilAttachmentIndex != -1)
      {        
        depthStencilAttachmentRef[i].attachment = subpasses[i].depthStencilAttachmentIndex;
        depthStencilAttachmentRef[i].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpassDescription[i].pDepthStencilAttachment = &depthStencilAttachmentRef[i];
      }      
    }

    //Dependencies
    std::vector<VkSubpassDependency> subpassDependencies( dependencyCount );
    for (uint32_t i = 0; i < dependencyCount; ++i)
    {
      subpassDependencies[i].srcSubpass = dependencies[i].srcSubpass;
      subpassDependencies[i].dstSubpass = dependencies[i].dstSubpass;
      subpassDependencies[i].srcStageMask = dependencies[i].srcStageMask;
      subpassDependencies[i].dstStageMask = dependencies[i].dstStageMask;
      subpassDependencies[i].srcAccessMask = dependencies[i].srcAccessMask;
      subpassDependencies[i].dstAccessMask = dependencies[i].dstAccessMask;
      subpassDependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = attachmentCount;
    renderPassCreateInfo.subpassCount = (uint32_t)subpassDescription.size();
    renderPassCreateInfo.pSubpasses = subpassDescription.data();
    renderPassCreateInfo.pAttachments = attachmentDescription.data();
    renderPassCreateInfo.dependencyCount = dependencyCount;
    renderPassCreateInfo.pDependencies = subpassDependencies.data();


    vkCreateRenderPass(context.device, &renderPassCreateInfo, nullptr, &renderPass->handle);
  }  
}

void render::renderPassDestroy(const context_t& context, render_pass_t* renderPass)
{
  delete[] renderPass->attachment;
  vkDestroyRenderPass(context.device, renderPass->handle, nullptr);
}

void render::frameBufferCreate(const context_t& context, uint32_t width, uint32_t height, const render_pass_t& renderPass, VkImageView* imageViews, frame_buffer_t* frameBuffer)
{ 
  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.attachmentCount = renderPass.attachmentCount;
  framebufferCreateInfo.pAttachments = imageViews;
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = 1;
  framebufferCreateInfo.renderPass = renderPass.handle;

  vkCreateFramebuffer(context.device, &framebufferCreateInfo, nullptr, &frameBuffer->handle);

  //No clear render pass
  render::render_pass_t::subpass_t noclearSubpass;
  std::vector<render_pass_t::attachment_t> noclearAttachments(renderPass.attachmentCount);
  for (uint32_t i(0); i < renderPass.attachmentCount; ++i)
  {
    noclearAttachments[i] = renderPass.attachment[i];
    noclearAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

    if(noclearAttachments[i].format == context.swapChain.depthStencil.format )
      noclearSubpass.depthStencilAttachmentIndex = i;
    else
      noclearSubpass.colorAttachmentIndex.push_back(i);
  }
  render::renderPassCreate(context, noclearAttachments.data(), (uint32_t)noclearAttachments.size(), &noclearSubpass, 1u, nullptr, 0u, &frameBuffer->renderPassNoClear);
  frameBuffer->renderPass = renderPass;
  frameBuffer->width = width;
  frameBuffer->height = height;
}


void render::frameBufferDestroy(const context_t& context, frame_buffer_t* frameBuffer)
{
  vkDestroyFramebuffer(context.device, frameBuffer->handle, nullptr);
  renderPassDestroy(context, &frameBuffer->renderPassNoClear);
}


void render::commandBufferCreate(const context_t& context, VkCommandBufferLevel level, VkSemaphore* waitSemaphore, VkPipelineStageFlags* waitStages, uint32_t waitSemaphoreCount, VkSemaphore* signalSemaphore, uint32_t signalSemaphoreCount, command_buffer_t::type_e type, command_buffer_t* commandBuffer)
{
  commandBuffer->type = type;
  commandBuffer->waitSemaphore = nullptr;
  commandBuffer->signalSemaphore = nullptr;
  commandBuffer->waitStages = nullptr;

  commandBuffer->waitSemaphoreCount = waitSemaphoreCount;  
  if(waitSemaphoreCount > 0)
  {
    commandBuffer->waitSemaphore = new VkSemaphore[waitSemaphoreCount];
    memcpy(commandBuffer->waitSemaphore, waitSemaphore, sizeof(VkSemaphore)*waitSemaphoreCount );
    commandBuffer->waitStages = new VkPipelineStageFlags[waitSemaphoreCount];
    memcpy(commandBuffer->waitStages, waitStages, sizeof(VkPipelineStageFlags)*waitSemaphoreCount);
  }
  

  commandBuffer->signalSemaphoreCount = signalSemaphoreCount;
  if (signalSemaphoreCount > 0)
  {
    commandBuffer->signalSemaphore = new VkSemaphore[signalSemaphoreCount];
    memcpy(commandBuffer->signalSemaphore, signalSemaphore, sizeof(VkSemaphore)*signalSemaphoreCount);

  }

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool;
  commandBufferAllocateInfo.level = level;
  vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &commandBuffer->handle );

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  vkCreateFence(context.device, &fenceCreateInfo, nullptr, &commandBuffer->fence);
}

void render::commandBufferDestroy(const context_t& context, command_buffer_t* commandBuffer )
{
  delete[] commandBuffer->waitSemaphore;
  delete[] commandBuffer->waitStages;
  delete[] commandBuffer->signalSemaphore;

  vkFreeCommandBuffers(context.device, context.commandPool, 1u, &commandBuffer->handle );
  vkDestroyFence(context.device, commandBuffer->fence, nullptr);
}

void render::commandBufferBegin(const context_t& context, const command_buffer_t& commandBuffer)
{
  vkWaitForFences(context.device, 1u, &commandBuffer.fence, VK_TRUE, UINT64_MAX);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  //Begin command buffer
  vkBeginCommandBuffer(commandBuffer.handle, &beginInfo);
}

void render::commandBufferRenderPassBegin(const context_t& context, const frame_buffer_t* frameBuffer, VkClearValue* clearValues, uint32_t clearValuesCount, const command_buffer_t& commandBuffer)
{ 
  //Begin render pass
  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

  renderPassBeginInfo.renderArea.extent = { frameBuffer->width , frameBuffer->height };

  
  renderPassBeginInfo.renderPass = frameBuffer->renderPass.handle;
  if (clearValues == nullptr)
    renderPassBeginInfo.renderPass = frameBuffer->renderPassNoClear.handle;
  renderPassBeginInfo.pClearValues = clearValues;
  renderPassBeginInfo.clearValueCount = clearValuesCount;

  //Begin render pass
  renderPassBeginInfo.framebuffer = frameBuffer->handle;
  vkCmdBeginRenderPass(commandBuffer.handle, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  //Set viewport and scissor rectangle
  VkViewport viewPort = { 0.0f, 0.0f, (float)frameBuffer->width, (float)frameBuffer->height, 0.0f, 1.0f };
  VkRect2D scissorRect = { { 0,0 },{ frameBuffer->width, frameBuffer->height } };
  vkCmdSetViewport(commandBuffer.handle, 0, 1, &viewPort);
  vkCmdSetScissor(commandBuffer.handle, 0, 1, &scissorRect);
  
}

void render::commandBufferNextSubpass(const command_buffer_t& commandBuffer)
{
  vkCmdNextSubpass(commandBuffer.handle, VK_SUBPASS_CONTENTS_INLINE);
}

void render::commandBufferRenderPassEnd(const command_buffer_t& commandBuffer)
{
  vkCmdEndRenderPass(commandBuffer.handle);  
}

void render::commandBufferEnd( const command_buffer_t& commandBuffer)
{
  vkEndCommandBuffer(commandBuffer.handle);
}


void render::commandBufferSubmit(const context_t& context, const command_buffer_t& commandBuffer )
{ 
  vkWaitForFences(context.device, 1u, &commandBuffer.fence, VK_TRUE, UINT64_MAX);
  vkResetFences(context.device, 1, &commandBuffer.fence);

  //Submit current command buffer
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = commandBuffer.waitSemaphoreCount;
  submitInfo.pWaitSemaphores = commandBuffer.waitSemaphore;
  submitInfo.signalSemaphoreCount = commandBuffer.signalSemaphoreCount;
  submitInfo.pSignalSemaphores = commandBuffer.signalSemaphore;
  submitInfo.pWaitDstStageMask = commandBuffer.waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer.handle;

  if(commandBuffer.type == command_buffer_t::GRAPHICS)
  {
    vkQueueSubmit(context.graphicsQueue.handle, 1, &submitInfo, commandBuffer.fence);
  }
  else
  {
    vkQueueSubmit(context.computeQueue.handle, 1, &submitInfo, commandBuffer.fence);
  }
}

VkSemaphore render::semaphoreCreate(const context_t& context)
{
  VkSemaphore semaphore;
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(context.device, &semaphoreCreateInfo, nullptr, &semaphore);
  return semaphore;
}

void render::semaphoreDestroy(const context_t& context, VkSemaphore semaphore)
{
  vkDestroySemaphore(context.device, semaphore, nullptr);
}


void render::textureCubemapCreateFromEquirectangularImage(const context_t& context, const image::image2D_t& image, uint32_t size, bool generateMipmaps, texture_t* cubemap)
{
  u32 mipLevels = generateMipmaps ? u32(1 + floor(log2(size))) : 1u;

  render::texture_t texture;
  render::texture2DCreate(context, &image, 1, render::texture_sampler_t(), &texture);
  render::textureCubemapCreate(context, VK_FORMAT_R32G32B32A32_SFLOAT, size, size, mipLevels, render::texture_sampler_t(), cubemap);

  mesh::mesh_t cube = mesh::unitCube(context);

  //Change cubemap layout for transfer
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = mipLevels;
  subresourceRange.layerCount = 6;
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, cubemap);

  //Create descriptor pool
  render::descriptor_pool_t descriptorPool;
  render::descriptorPoolCreate(context, 1u,
    render::combined_image_sampler_count(1u),
    render::uniform_buffer_count(0u),
    render::storage_buffer_count(0u),
    render::storage_image_count(0u),
    &descriptorPool);

  //Create pipeline
  render::graphics_pipeline_t pipeline;
  render::pipeline_layout_t pipelineLayout;
  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t bindings = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
  render::descriptorSetLayoutCreate(context, &bindings, 1u, &descriptorSetLayout);

  render::push_constant_range_t pushConstantsRange = { VK_SHADER_STAGE_VERTEX_BIT, sizeof(maths::mat4), 0u };
  render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, &pushConstantsRange, 1u, &pipelineLayout);

  render::render_pass_t renderPass = {};
  render::render_pass_t::attachment_t attachments = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_CLEAR };
  render::renderPassCreate(context, &attachments, 1u, nullptr, 0u, nullptr, 0u, &renderPass);

  //Load shaders
  render::shader_t vertexShader;
  const char* vsSource = R"(  
                                #version 440 core
                                layout(push_constant) uniform PushConstants
                                {
	                                layout (offset = 0) mat4 viewProjection;
                                }pushConstants;  
                                layout(location = 0) in vec3 aPosition;
                                layout(location = 0) out vec3 localPos;
                                void main(void)
                                {
                                  localPos = aPosition;
                                  gl_Position = pushConstants.viewProjection * vec4(aPosition,1.0);
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &vertexShader);

  render::shader_t fragmentShader;
  const char* fsSource = R"(  
                                #version 440 core
                                layout(location = 0) in vec3 localPos;
                                layout (set = 0, binding = 0) uniform sampler2D uTexture;
                                layout(location = 0) out vec4 color;
                                const vec2 invAtan = vec2(0.1591, 0.3183);
                                void main(void)
                                {
                                  vec3 direction = normalize( localPos );
                                  vec2 uv = vec2( atan(direction.z, direction.x), asin(direction.y) ) * invAtan + 0.5;
                                  color = texture( uTexture, uv );
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &fragmentShader);

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ size, size } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  render::graphicsPipelineCreate(context, renderPass.handle, 0u, cube.vertexFormat, pipelineLayout, pipelineDesc, &pipeline);

  //Create descriptor set
  render::descriptor_t textureDescriptor = render::getDescriptor(texture);
  render::descriptor_set_t descriptorSet;
  render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &textureDescriptor, &descriptorSet);

  //Create command buffer
  VkClearValue clearValue;
  clearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  render::command_buffer_t commandBuffer = {};
  render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::GRAPHICS, &commandBuffer);

  maths::mat4 projection = maths::perspectiveProjectionMatrix(1.57f, 1.0f, 0.1f, 1.0f);
  maths::mat4 view[6] = { maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(1.0f, 0.0f, 0.0f),  maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(-1.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f),  maths::vec3(0.0f, 0.0f, 1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, -1.0f, 0.0f), maths::vec3(0.0f, 0.0f,-1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, -1.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, 1.0f),  maths::vec3(0.0f, 1.0f, 0.0f)) };

  std::vector<render::frame_buffer_t> frameBuffers(mipLevels);
  std::vector<render::texture_t> renderTargets(mipLevels);
  u32 mipSize = size;
  for (u32 mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
  {
    //Create render target and framebuffer
    frameBuffers[mipLevel] = {};
    render::texture2DCreate(context, mipSize, mipSize, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, render::texture_sampler_t(), &renderTargets[mipLevel]);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);
    render::frameBufferCreate(context, mipSize, mipSize, renderPass, &renderTargets[mipLevel].imageView, &frameBuffers[mipLevel]);

    for (u32 i(0); i < 6; ++i)
    {
      maths::mat4 viewProjection = view[i] * projection;
      render::commandBufferBegin(context, commandBuffer);
      render::commandBufferRenderPassBegin(context, &frameBuffers[mipLevel], &clearValue, 1u, commandBuffer);

      render::pushConstants(commandBuffer, pipelineLayout, 0u, &viewProjection);
      render::graphicsPipelineBind(commandBuffer, pipeline);
      render::descriptorSetBind(commandBuffer, pipelineLayout, 0, &descriptorSet, 1u);
      mesh::draw(commandBuffer, cube);

      render::commandBufferRenderPassEnd(commandBuffer);

      //Copy render target to cubemap layer
      renderTargets[mipLevel].layout = VK_IMAGE_LAYOUT_UNDEFINED;
      render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &renderTargets[mipLevel]);
      render::textureCopy(commandBuffer, &renderTargets[mipLevel], cubemap, mipSize, mipSize, mipLevel, i);
      render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);
      
      render::commandBufferEnd(commandBuffer);
      render::commandBufferSubmit(context, commandBuffer);
    }

    mipSize /= 2;
  }

  //Change cubemap layout for shader access
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, cubemap);


  for (u32 i(0); i < mipLevels; ++i)
  {
    render::textureDestroy(context, &renderTargets[i]);
    render::frameBufferDestroy(context, &frameBuffers[i]);
  }

  render::textureDestroy(context, &texture);
  render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  render::pipelineLayoutDestroy(context, &pipelineLayout);
  render::renderPassDestroy(context, &renderPass);
  render::shaderDestroy(context, &vertexShader);
  render::shaderDestroy(context, &fragmentShader);
  render::graphicsPipelineDestroy(context, &pipeline);
  render::descriptorSetDestroy(context, &descriptorSet);
  render::commandBufferDestroy(context, &commandBuffer);
  render::descriptorPoolDestroy(context, &descriptorPool);
  mesh::destroy(context, &cube);
}

void render::diffuseConvolution(const context_t& context, texture_t environmentMap, uint32_t size, texture_t* irradiance)
{
  mesh::mesh_t cube = mesh::unitCube(context);

  render::textureCubemapCreate(context, VK_FORMAT_R32G32B32A32_SFLOAT, size, size, 1u, render::texture_sampler_t(), irradiance);
  //Change cubemap layout for transfer
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = 1;
  subresourceRange.layerCount = 6;
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, irradiance);

  //Create descriptor pool
  render::descriptor_pool_t descriptorPool;
  render::descriptorPoolCreate(context, 1u,
    render::combined_image_sampler_count(1u),
    render::uniform_buffer_count(0u),
    render::storage_buffer_count(0u),
    render::storage_image_count(0u),
    &descriptorPool);

  //Create pipeline
  render::graphics_pipeline_t pipeline;
  render::pipeline_layout_t pipelineLayout;
  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t bindings = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
  render::descriptorSetLayoutCreate(context, &bindings, 1u, &descriptorSetLayout);

  render::push_constant_range_t pushConstantsRange = { VK_SHADER_STAGE_VERTEX_BIT, sizeof(maths::mat4), 0u };
  render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, &pushConstantsRange, 1u, &pipelineLayout);

  render::render_pass_t renderPass = {};
  render::render_pass_t::attachment_t attachments = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_CLEAR };
  render::renderPassCreate(context, &attachments, 1u, nullptr, 0u, nullptr, 0u, &renderPass);

  //Create render target and framebuffer
  render::texture_t renderTarget;
  render::frame_buffer_t frameBuffer = {};
  render::texture2DCreate(context, size, size, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, render::texture_sampler_t(), &renderTarget);
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTarget);
  render::frameBufferCreate(context, size, size, renderPass, &renderTarget.imageView, &frameBuffer);

  //Load shaders
  render::shader_t vertexShader;
  const char* vsSource = R"(  
                                #version 440 core
                                layout(push_constant) uniform PushConstants
                                {
	                                layout (offset = 0) mat4 viewProjection;
                                }pushConstants;  
                                layout(location = 0) in vec3 aPosition;
                                layout(location = 0) out vec3 localPos;
                                void main(void)
                                {
                                  localPos = aPosition;
                                  gl_Position = pushConstants.viewProjection * vec4(aPosition,1.0);
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &vertexShader);

  render::shader_t fragmentShader;
  const char* fsSource = R"(  
                                #version 440 core
                                layout(location = 0) in vec3 localPos;
                                layout (set = 0, binding = 0) uniform samplerCube uTexture;
                                layout(location = 0) out vec4 color;
                                const vec2 invAtan = vec2(0.1591, 0.3183);
                                const float PI = 3.14159265359;
                                void main(void)
                                {
                                  vec3 normal = normalize( localPos );
                                  vec3 irradiance = vec3(0.0);  

                                  vec3 up    = vec3(0.0, 1.0, 0.0);
                                  vec3 right = normalize( cross(up, normal) );
                                  up         = normalize( cross(normal, right) );

                                  float sampleDelta = 0.025;
                                  float nrSamples = 0.0; 
                                  for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
                                  {
                                      for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
                                      {
                                          // spherical to cartesian (in tangent space)
                                          vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
                                          // tangent space to world
                                          vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal; 

                                          irradiance += textureLod(uTexture, sampleVec, 0).rgb * cos(theta) * sin(theta);
                                          nrSamples++;
                                      }
                                  }
                                  irradiance = PI * irradiance * (1.0 / float(nrSamples));                                  
                                  color = vec4(irradiance,1.0);
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &fragmentShader);

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ size, size } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  render::graphicsPipelineCreate(context, renderPass.handle, 0u, cube.vertexFormat, pipelineLayout, pipelineDesc, &pipeline);

  //Create descriptor set
  render::descriptor_t texture = render::getDescriptor(environmentMap);
  render::descriptor_set_t descriptorSet;
  render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &texture, &descriptorSet);

  //Create command buffer
  VkClearValue clearValue;
  clearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  render::command_buffer_t commandBuffer = {};
  render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::GRAPHICS, &commandBuffer);

  maths::mat4 projection = maths::perspectiveProjectionMatrix(1.57f, 1.0f, 0.1f, 1.0f);
  maths::mat4 view[6] = { maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(1.0f, 0.0f, 0.0f),  maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(-1.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f),  maths::vec3(0.0f, 0.0f, 1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, -1.0f, 0.0f), maths::vec3(0.0f, 0.0f,-1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, -1.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, 1.0f),  maths::vec3(0.0f, 1.0f, 0.0f)) };

  for (u32 i(0); i<6; ++i)
  {
    maths::mat4 viewProjection = view[i] * projection;
    render::commandBufferBegin(context, commandBuffer);
    render::commandBufferRenderPassBegin(context, &frameBuffer, &clearValue, 1u, commandBuffer);

    render::pushConstants(commandBuffer, pipelineLayout, 0u, &viewProjection);
    render::graphicsPipelineBind(commandBuffer, pipeline);
    render::descriptorSetBind(commandBuffer, pipelineLayout, 0, &descriptorSet, 1u);
    mesh::draw(commandBuffer, cube);
    render::commandBufferRenderPassEnd(commandBuffer);

    //Copy render target to cubemap layer
    renderTarget.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &renderTarget);
    render::textureCopy(commandBuffer, &renderTarget, irradiance, size, size, 0, i);
    render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTarget);

    
    render::commandBufferEnd(commandBuffer);
    render::commandBufferSubmit(context, commandBuffer);
  }

  //Change cubemap layout for shader access
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, irradiance);

  //Clean-up
  render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  render::pipelineLayoutDestroy(context, &pipelineLayout);
  render::renderPassDestroy(context, &renderPass);
  render::textureDestroy(context, &renderTarget);
  render::frameBufferDestroy(context, &frameBuffer);
  render::shaderDestroy(context, &vertexShader);
  render::shaderDestroy(context, &fragmentShader);
  render::graphicsPipelineDestroy(context, &pipeline);
  render::descriptorSetDestroy(context, &descriptorSet);
  render::commandBufferDestroy(context, &commandBuffer);
  render::descriptorPoolDestroy(context, &descriptorPool);
  mesh::destroy(context, &cube);
}

void render::specularConvolution(const context_t& context, texture_t environmentMap, uint32_t size, uint32_t maxMipmapLevels, texture_t* specularMap)
{
  mesh::mesh_t cube = mesh::unitCube(context);

  u32 mipLevels = maths::minValue((u32)(1 + floor(log2(size))), maxMipmapLevels);
  render::textureCubemapCreate(context, VK_FORMAT_R32G32B32A32_SFLOAT, size, size, mipLevels, render::texture_sampler_t(), specularMap);
  //Change cubemap layout for transfer
  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = mipLevels;
  subresourceRange.layerCount = 6;
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, specularMap);

  //Create descriptor pool
  render::descriptor_pool_t descriptorPool;
  render::descriptorPoolCreate(context, 1u,
    render::combined_image_sampler_count(1u),
    render::uniform_buffer_count(0u),
    render::storage_buffer_count(0u),
    render::storage_image_count(0u),
    &descriptorPool);

  //Create pipeline
  render::graphics_pipeline_t pipeline;
  render::pipeline_layout_t pipelineLayout;
  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t bindings = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
  render::descriptorSetLayoutCreate(context, &bindings, 1u, &descriptorSetLayout);

  struct push_constants_t
  {
    maths::mat4 viewProjection;
    float roughness;
    float sourceCubemapResolution;
  }pushConstants;

  pushConstants.sourceCubemapResolution = (float)environmentMap.extent.width;

  render::push_constant_range_t pushConstantsRange = { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(pushConstants), 0u };
  render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, &pushConstantsRange, 1u, &pipelineLayout);

  render::render_pass_t::attachment_t attachments = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_CLEAR };
  render::render_pass_t renderPass = {};
  render::renderPassCreate(context, &attachments, 1u, nullptr, 0u, nullptr, 0u, &renderPass);

  //Load shaders
  render::shader_t vertexShader;
  const char* vsSource = R"(  
                                #version 440 core
                                layout(push_constant) uniform PushConstants
                                {
	                                layout (offset = 0) mat4 viewProjection;
                                  layout (offset = 64) float roughness;
                                  layout (offset = 68) float sourceCubemapResolution;
                                }pushConstants;
                                layout(location = 0) in vec3 aPosition;
                                layout(location = 0) out vec3 localPos;
                                void main(void)
                                {
                                  localPos = aPosition;
                                  gl_Position = pushConstants.viewProjection * vec4(aPosition,1.0);
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &vertexShader);

  render::shader_t fragmentShader;
  const char* fsSource = R"(  
                                #version 440 core
                                layout(push_constant) uniform PushConstants
                                {
	                                layout (offset = 0) mat4 viewProjection;
                                  layout (offset = 64) float roughness;
                                  layout (offset = 68) float sourceCubemapResolution;
                                }pushConstants;

                                layout(location = 0) in vec3 localPos;
                                layout (set = 0, binding = 0) uniform samplerCube uTexture;
                                layout(location = 0) out vec4 color;
                                const vec2 invAtan = vec2(0.1591, 0.3183);
                                const float PI = 3.14159265359;

                                float RadicalInverse_VdC(uint bits) 
                                {
                                    bits = (bits << 16u) | (bits >> 16u);
                                    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                                    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                                    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                                    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                                    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
                                }

                                vec2 Hammersley(uint i, uint N)
                                {
                                    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
                                }

                                vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
                                {
                                    float a = roughness*roughness;
	
                                    float phi = 2.0 * PI * Xi.x;
                                    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
                                    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
                                    // from spherical coordinates to cartesian coordinates
                                    vec3 H;
                                    H.x = cos(phi) * sinTheta;
                                    H.y = sin(phi) * sinTheta;
                                    H.z = cosTheta;
	
                                    // from tangent-space vector to world-space sample vector
                                    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
                                    vec3 tangent   = normalize(cross(up, N));
                                    vec3 bitangent = cross(N, tangent);
	
                                    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
                                    return normalize(sampleVec);
                                }

                                float DistributionGGX(vec3 N, vec3 H, float roughness)
                                {
                                  float a = roughness*roughness;
                                  float a2 = a*a;
                                  float NdotH = max(dot(N, H), 0.0);
                                  float NdotH2 = NdotH*NdotH;
                                  float nom = a2;
                                  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
                                  denom = PI * denom * denom;
                                  return nom / denom;
                                }

                                void main()
                                {		
                                    vec3 N = normalize(localPos);    
                                    vec3 R = N;
                                    vec3 V = R;

                                    const uint SAMPLE_COUNT = 1024u;
                                    float totalWeight = 0.0;   
                                    vec3 prefilteredColor = vec3(0.0);     
                                    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
                                    {
                                        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                                        vec3 H  = ImportanceSampleGGX(Xi, N, pushConstants.roughness);
                                        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

                                        float NdotL = max(dot(N, L), 0.0);
                                        if(NdotL > 0.0)
                                        {
                                            float D   = DistributionGGX(N, H, pushConstants.roughness);
                                            float NdotH = max(dot(N, H), 0.0);
                                            float HdotV = max(dot(H, V), 0.0);
                                            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 

                                            float resolution = pushConstants.sourceCubemapResolution; // resolution of source cubemap (per face)
                                            float saTexel  = 4.0 * PI / (6.0 * resolution * resolution);
                                            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

                                            float mipLevel = pushConstants.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 
                                            prefilteredColor += textureLod(uTexture, L, mipLevel).rgb * NdotL;
                                            totalWeight      += NdotL;
                                        }
                                    }
                                    prefilteredColor = prefilteredColor / totalWeight;
                                    color = vec4(prefilteredColor, 1.0);
                                }
                          )";
  render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &fragmentShader);

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ size, size } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_FRONT_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  render::graphicsPipelineCreate(context, renderPass.handle, 0u, cube.vertexFormat, pipelineLayout, pipelineDesc, &pipeline);

  //Create descriptor set
  render::descriptor_t texture = render::getDescriptor(environmentMap);
  render::descriptor_set_t descriptorSet;
  render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &texture, &descriptorSet);

  //Create command buffer
  VkClearValue clearValue;
  clearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  render::command_buffer_t commandBuffer = {};
  render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::GRAPHICS, &commandBuffer);

  maths::mat4 projection = maths::perspectiveProjectionMatrix(1.57f, 1.0f, 0.1f, 1.0f);
  maths::mat4 view[6] = { maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(1.0f, 0.0f, 0.0f),  maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(-1.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 1.0f, 0.0f),  maths::vec3(0.0f, 0.0f, 1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, -1.0f, 0.0f), maths::vec3(0.0f, 0.0f,-1.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, -1.0f), maths::vec3(0.0f, 1.0f, 0.0f)),
                          maths::lookAtMatrix(maths::vec3(0.0f, 0.0f, 0.0f), maths::vec3(0.0f, 0.0f, 1.0f),  maths::vec3(0.0f, 1.0f, 0.0f)) };

  std::vector<render::frame_buffer_t> frameBuffers(mipLevels);
  std::vector<render::texture_t> renderTargets(mipLevels);
  u32 mipSize = size;
  for (u32 mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
  {
    //Create render target and framebuffer
    frameBuffers[mipLevel] = {};
    render::texture2DCreate(context, mipSize, mipSize, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, render::texture_sampler_t(), &renderTargets[mipLevel]);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);
    render::frameBufferCreate(context, mipSize, mipSize, renderPass, &renderTargets[mipLevel].imageView, &frameBuffers[mipLevel]);

    pushConstants.roughness = (float)mipLevel / (float)(mipLevels - 1);
    for (u32 i(0); i < 6; ++i)
    {
      pushConstants.viewProjection = view[i] * projection;

      render::commandBufferBegin(context, commandBuffer);
      render::commandBufferRenderPassBegin(context, &frameBuffers[mipLevel], &clearValue, 1u, commandBuffer);
      render::pushConstants(commandBuffer, pipelineLayout, 0u, &pushConstants);
      render::graphicsPipelineBind(commandBuffer, pipeline);
      render::descriptorSetBind(commandBuffer, pipelineLayout, 0, &descriptorSet, 1u);
      mesh::draw(commandBuffer, cube);
      render::commandBufferRenderPassEnd(commandBuffer);

      //Copy render target to cubemap layer
      renderTargets[mipLevel].layout = VK_IMAGE_LAYOUT_UNDEFINED;
      render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &renderTargets[mipLevel]);
      render::textureCopy(commandBuffer, &renderTargets[mipLevel], specularMap, mipSize, mipSize, mipLevel, i);      
      render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);
      
      render::commandBufferEnd(commandBuffer);
      render::commandBufferSubmit(context, commandBuffer);
    }
    mipSize /= 2;
  }

  //Change cubemap layout for shader access
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, specularMap);

  //Clean-up
  for (u32 i(0); i < mipLevels; ++i)
  {
    render::textureDestroy(context, &renderTargets[i]);
    render::frameBufferDestroy(context, &frameBuffers[i]);
  }

  render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  render::pipelineLayoutDestroy(context, &pipelineLayout);
  render::renderPassDestroy(context, &renderPass);

  render::shaderDestroy(context, &vertexShader);
  render::shaderDestroy(context, &fragmentShader);
  render::graphicsPipelineDestroy(context, &pipeline);
  render::descriptorSetDestroy(context, &descriptorSet);
  render::commandBufferDestroy(context, &commandBuffer);
  render::descriptorPoolDestroy(context, &descriptorPool);
  mesh::destroy(context, &cube);
}

void render::waitForAllCommandBuffersToFinish(const context_t& context)
{
  //Create command buffer
  VkCommandBuffer commandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &commandBuffer);

  //Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);
  vkEndCommandBuffer(commandBuffer);

  ////Queue commandBuffer for execution
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;
  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VkFence fence;
  vkCreateFence(context.device, &fenceCreateInfo, nullptr, &fence);
  vkResetFences(context.device, 1u, &fence);
  vkQueueSubmit(context.graphicsQueue.handle, 1, &submitInfo, fence);
  
  //Destroy command buffer
  vkWaitForFences(context.device, 1u, &fence, VK_TRUE, UINT64_MAX);
  vkFreeCommandBuffers(context.device, context.commandPool, 1, &commandBuffer);
  vkDestroyFence(context.device, fence, nullptr);
}

void render::brdfConvolution(const context_t& context, uint32_t size, texture_t* brdfConvolution)
{
  mesh::mesh_t quad = mesh::fullScreenQuad(context);
  render::texture2DCreate(context, size, size, 1u, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, render::texture_sampler_t(), brdfConvolution);
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, brdfConvolution);

  //Create pipeline    
  render::pipeline_layout_t pipelineLayout;
  render::pipelineLayoutCreate(context, nullptr, 0u, nullptr, 0u, &pipelineLayout);

  render::render_pass_t::attachment_t attachments = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_CLEAR };
  render::render_pass_t renderPass = {};
  render::renderPassCreate(context, &attachments, 1u, nullptr, 0u, nullptr, 0u, &renderPass);

  //Load shaders
  render::shader_t vertexShader;
  const char* vsSource = R"(  
                                #version 440 core
                                layout(location = 0) in vec3 aPosition;
                                layout(location = 1) in vec2 aTexCoord;
                                layout(location = 0) out vec2 uv;
                                void main(void)
                                {
                                  gl_Position = vec4(aPosition,1.0);
                                  uv = aTexCoord;
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &vertexShader);

  render::shader_t fragmentShader;
  const char* fsSource = R"(  
                                #version 440 core
                                layout(location = 0) out vec4 color;
                                layout(location = 0) in vec2 uv;

                                const float PI = 3.14159265359;
                                float GeometrySchlickGGX(float NdotV, float roughness)
                                {
                                    float a = roughness;
                                    float k = (a * a) / 2.0;

                                    float nom   = NdotV;
                                    float denom = NdotV * (1.0 - k) + k;

                                    return nom / denom;
                                }

                                float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
                                {
                                    float NdotV = max(dot(N, V), 0.0);
                                    float NdotL = max(dot(N, L), 0.0);
                                    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
                                    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

                                    return ggx1 * ggx2;
                                }  

                                float RadicalInverse_VdC(uint bits) 
                                {
                                    bits = (bits << 16u) | (bits >> 16u);
                                    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                                    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                                    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                                    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                                    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
                                }

                                vec2 Hammersley(uint i, uint N)
                                {
                                    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
                                }

                                vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
                                {
                                    float a = roughness*roughness;
	
                                    float phi = 2.0 * PI * Xi.x;
                                    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
                                    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
                                    // from spherical coordinates to cartesian coordinates
                                    vec3 H;
                                    H.x = cos(phi) * sinTheta;
                                    H.y = sin(phi) * sinTheta;
                                    H.z = cosTheta;
	
                                    // from tangent-space vector to world-space sample vector
                                    vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
                                    vec3 tangent   = normalize(cross(up, N));
                                    vec3 bitangent = cross(N, tangent);
	
                                    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
                                    return normalize(sampleVec);
                                }  

                                vec2 IntegrateBRDF(float NdotV, float roughness)
                                {
                                    vec3 V;
                                    V.x = sqrt(1.0 - NdotV*NdotV);
                                    V.y = 0.0;
                                    V.z = NdotV;

                                    float A = 0.0;
                                    float B = 0.0;

                                    vec3 N = vec3(0.0, 0.0, 1.0);

                                    const uint SAMPLE_COUNT = 1024u;
                                    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
                                    {
                                        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
                                        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
                                        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

                                        float NdotL = max(L.z, 0.0);
                                        float NdotH = max(H.z, 0.0);
                                        float VdotH = max(dot(V, H), 0.0);

                                        if(NdotL > 0.0)
                                        {
                                            float G = GeometrySmith(N, V, L, roughness);
                                            float G_Vis = (G * VdotH) / (NdotH * NdotV);
                                            float Fc = pow(1.0 - VdotH, 5.0);

                                            A += (1.0 - Fc) * G_Vis;
                                            B += Fc * G_Vis;
                                        }
                                    }
                                    A /= float(SAMPLE_COUNT);
                                    B /= float(SAMPLE_COUNT);
                                    return vec2(A, B);
                                }
                                void main()
                                {
                                  vec2 integratedBRDF = IntegrateBRDF(uv.x, 1.0-uv.y);
                                  color = vec4(integratedBRDF,0,0);
                                }
                          )";
  render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &fragmentShader);

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ size, size } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  render::graphics_pipeline_t pipeline = {};
  render::graphicsPipelineCreate(context, renderPass.handle, 0u, quad.vertexFormat, pipelineLayout, pipelineDesc, &pipeline);

  VkClearValue clearValue;
  clearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  render::command_buffer_t commandBuffer = {};
  render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::GRAPHICS, &commandBuffer);

  //Create render target and framebuffer
  render::frame_buffer_t frameBuffer = {};
  render::frameBufferCreate(context, size, size, renderPass, &brdfConvolution->imageView, &frameBuffer);

  render::commandBufferBegin(context, commandBuffer);
  render::commandBufferRenderPassBegin(context, &frameBuffer, &clearValue, 1u, commandBuffer);
  render::graphicsPipelineBind(commandBuffer, pipeline);
  mesh::draw(commandBuffer, quad);

  render::commandBufferRenderPassEnd(commandBuffer);

  //Change cubemap layout for shader access
  brdfConvolution->layout = VK_IMAGE_LAYOUT_UNDEFINED;
  render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, brdfConvolution);

  render::commandBufferEnd(commandBuffer);
  render::commandBufferSubmit(context, commandBuffer);

  waitForAllCommandBuffersToFinish(context);

  //Clean-up
  render::frameBufferDestroy(context, &frameBuffer);
  render::pipelineLayoutDestroy(context, &pipelineLayout);
  render::renderPassDestroy(context, &renderPass);

  render::shaderDestroy(context, &vertexShader);
  render::shaderDestroy(context, &fragmentShader);
  render::graphicsPipelineDestroy(context, &pipeline);
  render::commandBufferDestroy(context, &commandBuffer);
  mesh::destroy(context, &quad);
}

void render::texture2DCreateAndGenerateMipmaps(const context_t& context, const image::image2D_t& image, texture_sampler_t sampler, texture_t* texture)
{
  mesh::mesh_t quad = mesh::fullScreenQuad(context);

  u32 size = maths::minValue(image.width, image.height);
  u32 mipLevels = (u32)(1 + floor(log2(size)));

  VkFormat format = getImageFormat(image);
  render::texture2DCreate(context, size, size, mipLevels, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, render::texture_sampler_t(), texture);

  VkImageSubresourceRange subresourceRange = {};
  subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  subresourceRange.baseMipLevel = 0;
  subresourceRange.levelCount = mipLevels;
  subresourceRange.layerCount = 1;
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, texture);

  texture_t inputTexture;
  render::texture2DCreate(context, &image, 1u, texture_sampler_t(), &inputTexture);


  //Create descriptor pool
  render::descriptor_pool_t descriptorPool;
  render::descriptorPoolCreate(context, 1u,
    render::combined_image_sampler_count(1u + mipLevels ),
    render::uniform_buffer_count(0u),
    render::storage_buffer_count(0u),
    render::storage_image_count(0u),
    &descriptorPool);

  render::descriptor_set_layout_t descriptorSetLayout;
  render::descriptor_binding_t bindings = { render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER, 0, VK_SHADER_STAGE_FRAGMENT_BIT };
  render::descriptorSetLayoutCreate(context, &bindings, 1u, &descriptorSetLayout);

  //Create descriptor set
  render::descriptor_t inputTextureDesc = render::getDescriptor(inputTexture);
  render::descriptor_set_t descriptorSet;
  render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &inputTextureDesc, &descriptorSet);


  //Create pipeline    
  render::pipeline_layout_t pipelineLayout;
  render::pipelineLayoutCreate(context, &descriptorSetLayout, 1u, nullptr, 0u, &pipelineLayout);

  render::render_pass_t::attachment_t attachments = { format, VK_SAMPLE_COUNT_1_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
    VK_ATTACHMENT_STORE_OP_STORE,VK_ATTACHMENT_LOAD_OP_CLEAR };
  render::render_pass_t renderPass = {};
  render::renderPassCreate(context, &attachments, 1u, nullptr, 0u, nullptr, 0u, &renderPass);

  //Load shaders
  render::shader_t vertexShader;
  const char* vsSource = R"(  
                                #version 440 core
                                layout(location = 0) in vec3 aPosition;
                                layout(location = 1) in vec2 aTexCoord;
                                layout(location = 0) out vec2 uv;
                                void main(void)
                                {
                                  gl_Position = vec4(aPosition,1.0);
                                  uv = aTexCoord;
                                })";
  render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, vsSource, &vertexShader);

  render::shader_t fragmentShader;
  const char* fsSource = R"(  
                                #version 440 core
                                layout(location = 0) out vec4 color;
                                layout(location = 0) in vec2 uv;
                                layout (set = 0, binding = 0) uniform sampler2D uTexture;
                                void main()
                                {                                  
                                  color = texture(uTexture,uv);
                                }
                          )";
  render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, fsSource, &fragmentShader);

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.viewPort = { 0.0f, 0.0f, (float)size, (float)size, 0.0f, 1.0f };
  pipelineDesc.scissorRect = { { 0,0 },{ size, size } };
  pipelineDesc.blendState.resize(1);
  pipelineDesc.blendState[0].colorWriteMask = 0xF;
  pipelineDesc.blendState[0].blendEnable = VK_FALSE;
  pipelineDesc.cullMode = VK_CULL_MODE_BACK_BIT;
  pipelineDesc.depthTestEnabled = false;
  pipelineDesc.depthWriteEnabled = false;
  pipelineDesc.vertexShader = vertexShader;
  pipelineDesc.fragmentShader = fragmentShader;
  render::graphics_pipeline_t pipeline = {};
  render::graphicsPipelineCreate(context, renderPass.handle, 0u, quad.vertexFormat, pipelineLayout, pipelineDesc, &pipeline);

  VkClearValue clearValue;
  clearValue.color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

  render::command_buffer_t commandBuffer = {};
  render::commandBufferCreate(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY, nullptr, nullptr, 0u, nullptr, 0u, render::command_buffer_t::GRAPHICS, &commandBuffer);


  std::vector<render::frame_buffer_t> frameBuffers(mipLevels);
  std::vector<render::texture_t> renderTargets(mipLevels);
  u32 mipSize = size;
  for (u32 mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
  {
    //Create render target and framebuffer
    frameBuffers[mipLevel] = {};
    render::texture2DCreate(context, mipSize, mipSize, 1u, format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, render::texture_sampler_t(), &renderTargets[mipLevel]);
    render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);
    render::frameBufferCreate(context, mipSize, mipSize, renderPass, &renderTargets[mipLevel].imageView, &frameBuffers[mipLevel]);

    if (mipLevel > 0)
    {
      render::descriptorSetDestroy(context, &descriptorSet);
      descriptor_t descriptor = getDescriptor(renderTargets[mipLevel - 1]);
      render::descriptorSetCreate(context, descriptorPool, descriptorSetLayout, &descriptor, &descriptorSet);
    }

    render::commandBufferBegin(context, commandBuffer);
    render::commandBufferRenderPassBegin(context, &frameBuffers[mipLevel], &clearValue, 1u, commandBuffer);
    render::graphicsPipelineBind(commandBuffer, pipeline);
    render::descriptorSetBind(commandBuffer, pipelineLayout, 0, &descriptorSet, 1u);
    mesh::draw(commandBuffer, quad);
    render::commandBufferRenderPassEnd(commandBuffer);

    //Copy render target to mip level
    renderTargets[mipLevel].layout = VK_IMAGE_LAYOUT_UNDEFINED;
    render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &renderTargets[mipLevel]);
    render::textureCopy(commandBuffer, &renderTargets[mipLevel], texture, mipSize, mipSize, mipLevel);
    render::textureChangeLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &renderTargets[mipLevel]);

    render::commandBufferEnd(commandBuffer);
    render::commandBufferSubmit(context, commandBuffer);

    mipSize /= 2;
  }

  //Change cubemap layout for shader access
  render::textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, subresourceRange, texture);

    
  //Clean-up
  for (u32 i(0); i < mipLevels; ++i)
  {
    render::textureDestroy(context, &renderTargets[i]);
    render::frameBufferDestroy(context, &frameBuffers[i]);
  }

  render::textureDestroy(context, &inputTexture);
  render::descriptorSetLayoutDestroy(context, &descriptorSetLayout);
  render::descriptorSetDestroy(context, &descriptorSet);  
  render::pipelineLayoutDestroy(context, &pipelineLayout);
  render::renderPassDestroy(context, &renderPass);
  render::shaderDestroy(context, &vertexShader);
  render::shaderDestroy(context, &fragmentShader);
  render::graphicsPipelineDestroy(context, &pipeline);
  render::commandBufferDestroy(context, &commandBuffer);
  render::descriptorPoolDestroy(context, &descriptorPool);
  mesh::destroy(context, &quad);
}
