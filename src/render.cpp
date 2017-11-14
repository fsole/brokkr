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

#include "render.h"
#include "mesh.h"

#include <cstring>  //memcpy
#include <cassert>

using namespace bkk;
using namespace bkk::render;

//#define VK_DEBUG_LAYERS

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

  std::cerr << "VULKAN_ERROR: " << msg << std::endl;
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
  applicationInfo.pApplicationName = "Vulkan Sample application";
  applicationInfo.pEngineName = "VulkanFramework";
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

  std::vector<VkPhysicalDevice> devices{ physicalDeviceCount };
  vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, devices.data());

  //Find a physical device with graphics and compute queues
  *physicalDevice = nullptr;
  graphicsQueue->queueIndex_ = -1;
  computeQueue->queueIndex_ = -1;
  for (uint32_t i(0); i < physicalDeviceCount; ++i)
  {
    graphicsQueue->queueIndex_ = GetQueueIndex(&devices[i], VK_QUEUE_GRAPHICS_BIT);
    computeQueue->queueIndex_ = GetQueueIndex(&devices[i], VK_QUEUE_COMPUTE_BIT);
    if (graphicsQueue->queueIndex_ != -1 && computeQueue->queueIndex_ != -1)
    {
      *physicalDevice = devices[i];
      break;
    }
  }

  assert(*physicalDevice);

  VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
  deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfo.queueCount = 1;
  deviceQueueCreateInfo.queueFamilyIndex = graphicsQueue->queueIndex_;

  static const float queuePriorities[] = { 1.0f };
  deviceQueueCreateInfo.pQueuePriorities = queuePriorities;

  VkDeviceCreateInfo deviceCreateInfo = {};
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;

  deviceCreateInfo.ppEnabledLayerNames = NULL;
  deviceCreateInfo.enabledLayerCount = 0u;

  std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };

  deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
  deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t> (deviceExtensions.size());

  *logicalDevice = nullptr;
  vkCreateDevice(*physicalDevice, &deviceCreateInfo, nullptr, logicalDevice);
  assert(*logicalDevice);

  graphicsQueue->handle_ = nullptr;
  vkGetDeviceQueue(*logicalDevice, graphicsQueue->queueIndex_, 0, &graphicsQueue->handle_);
  assert(graphicsQueue->handle_);

  computeQueue->handle_ = nullptr;
  vkGetDeviceQueue(*logicalDevice, computeQueue->queueIndex_, 0, &computeQueue->handle_);
  assert(computeQueue->handle_);
}

static VkBool32 GetDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
{
  std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT,
                                         VK_FORMAT_D32_SFLOAT,
                                         VK_FORMAT_D24_UNORM_S8_UINT,
                                         VK_FORMAT_D16_UNORM_S8_UINT,
                                         VK_FORMAT_D16_UNORM };

  for (size_t i(0); i < depthFormats.size(); ++i)
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
  depthStencilBuffer->format_ = format;

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
  vkCreateImage(context->device_, &imageCreateInfo, nullptr, &depthStencilBuffer->image_);

  //Allocate and bind memory for the image.
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context->device_, depthStencilBuffer->image_, &requirements);
  depthStencilBuffer->memory_ = gpuMemoryAllocate(*context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context->device_, depthStencilBuffer->image_, depthStencilBuffer->memory_.handle_, depthStencilBuffer->memory_.offset_);

  //Create command buffer
  VkCommandBuffer commandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context->commandPool_;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context->device_, &commandBufferAllocateInfo, &commandBuffer);

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
  imageBarrier.image = depthStencilBuffer->image_;
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
  vkResetFences(context->device_, 1u, &context->swapChain_.frameFence_[0]);
  vkQueueSubmit(context->graphicsQueue_.handle_, 1, &submitInfo, context->swapChain_.frameFence_[0]);

  //Destroy command buffer
  vkWaitForFences(context->device_, 1u, &context->swapChain_.frameFence_[0], VK_TRUE, UINT64_MAX);
  vkFreeCommandBuffers(context->device_, context->commandPool_, 1, &commandBuffer);

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
  imageViewCreateInfo.image = depthStencilBuffer->image_;
  vkCreateImageView(context->device_, &imageViewCreateInfo, nullptr, &depthStencilBuffer->imageView_);
}


static void CreateSurface(VkInstance instance, VkPhysicalDevice physicalDevice, const window::window_t& window, const context_t& context, surface_t* surface)
{
#ifdef WIN32
  VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surfaceCreateInfo.hinstance = window.instance_;
  surfaceCreateInfo.hwnd = window.handle_;
  vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface->handle_);
#else
  VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
  surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  surfaceCreateInfo.connection = window->connection_;
  surfaceCreateInfo.window = window->handle_;
  vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface->handle_);
#endif


  //Check if presentation is supported by the surface
  VkBool32 presentSupported;
  context.vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, 0, surface->handle_, &presentSupported);
  assert(presentSupported);

  //Get surface capabilities
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  context.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface->handle_, &surfaceCapabilities);

  if (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
  {
    surface->preTransform_ = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  }
  else
  {
    surface->preTransform_ = surfaceCapabilities.currentTransform;
  }

  //Get surface format and color space
  uint32_t surfaceFormatCount = 0;
  context.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->handle_, &surfaceFormatCount, nullptr);
  assert(surfaceFormatCount > 0);
  std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
  context.vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface->handle_, &surfaceFormatCount, surfaceFormats.data());

  surface->imageFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
  if (surfaceFormats[0].format != VK_FORMAT_UNDEFINED)
  {
    surface->imageFormat_ = surfaceFormats[0].format;
  }

  surface->colorSpace_ = surfaceFormats.front().colorSpace;
}

static void CreateSwapChain(context_t* context,
  uint32_t width, uint32_t height,
  uint32_t imageCount)
{

  VkExtent2D swapChainSize = { width, height };
  context->swapChain_.imageWidth_ = width;
  context->swapChain_.imageHeight_ = height;
  context->swapChain_.imageCount_ = imageCount;
  context->swapChain_.currentImage_ = 0;

  //Create the swapchain
  VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
  swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainCreateInfo.surface = context->surface_.handle_;
  swapchainCreateInfo.minImageCount = imageCount;
  swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchainCreateInfo.imageColorSpace = context->surface_.colorSpace_;
  swapchainCreateInfo.imageFormat = context->surface_.imageFormat_;
  swapchainCreateInfo.pQueueFamilyIndices = nullptr;
  swapchainCreateInfo.queueFamilyIndexCount = 0;
  swapchainCreateInfo.clipped = VK_TRUE;
  swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
  swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainCreateInfo.imageExtent = swapChainSize;
  swapchainCreateInfo.imageArrayLayers = 1;
  swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  context->vkCreateSwapchainKHR(context->device_, &swapchainCreateInfo, nullptr, &context->swapChain_.handle_);

  //Get the maximum number of images supported by the swapchain
  uint32_t maxImageCount = 0;
  context->vkGetSwapchainImagesKHR(context->device_, context->swapChain_.handle_, &maxImageCount, nullptr);

  //Create the swapchain images
  assert(imageCount <= maxImageCount);
  context->swapChain_.image_.resize(imageCount);
  context->vkGetSwapchainImagesKHR(context->device_, context->swapChain_.handle_, &maxImageCount, context->swapChain_.image_.data());

  //Create an imageview for each image
  context->swapChain_.imageView_.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    VkImageViewCreateInfo imageViewCreateInfo = {};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = context->swapChain_.image_[i];
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = context->surface_.imageFormat_;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.layerCount = 1;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCreateImageView(context->device_, &imageViewCreateInfo, nullptr, &context->swapChain_.imageView_[i]);
  }

  //Create presentation command buffers
  context->swapChain_.commandBuffer_.resize(imageCount);
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = imageCount;
  commandBufferAllocateInfo.commandPool = context->commandPool_;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context->device_, &commandBufferAllocateInfo, context->swapChain_.commandBuffer_.data());

  //Create fences
  context->swapChain_.frameFence_.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(context->device_, &fenceCreateInfo, nullptr, &context->swapChain_.frameFence_[i]);
  }


  //Create depth stencil buffer (shared by all the framebuffers)
  VkFormat depthStencilFormat = VK_FORMAT_UNDEFINED;
  GetDepthStencilFormat(context->physicalDevice_, &depthStencilFormat);
  CreateDepthStencilBuffer(context, width, height, depthStencilFormat, &context->swapChain_.depthStencil_);

  //Create the presentation render pass
  context->swapChain_.renderPass_ = CreatePresentationRenderPass(context->device_, context->surface_.imageFormat_, depthStencilFormat);

  //Create frame buffers
  context->swapChain_.frameBuffer_.resize(imageCount);
  VkImageView attachments[2];
  attachments[1] = context->swapChain_.depthStencil_.imageView_;
  for (uint32_t i = 0; i < imageCount; ++i)
  {
    attachments[0] = context->swapChain_.imageView_[i];
    VkFramebufferCreateInfo framebufferCreateInfo = {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.attachmentCount = 2;
    framebufferCreateInfo.pAttachments = attachments;
    framebufferCreateInfo.width = width;
    framebufferCreateInfo.height = height;
    framebufferCreateInfo.layers = 1;
    framebufferCreateInfo.renderPass = context->swapChain_.renderPass_;

    vkCreateFramebuffer(context->device_, &framebufferCreateInfo, nullptr, &context->swapChain_.frameBuffer_[i]);
  }
}

static void CreateSemaphores(VkDevice device, swapchain_t* swapChain)
{
  //Create semaphores
  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &swapChain->imageAcquired_);
  vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &swapChain->renderingComplete_);
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
  context->instance_ = CreateInstance(applicationName, engineName);
  CreateDeviceAndQueues(context->instance_, &context->physicalDevice_, &context->device_, &context->graphicsQueue_, &context->computeQueue_);

  //Get memory properties of the physical device
  vkGetPhysicalDeviceMemoryProperties(context->physicalDevice_, &context->memoryProperties_);

  context->commandPool_ = CreateCommandPool(context->device_, context->graphicsQueue_.queueIndex_);
  
  ImportFunctions(context->instance_, context->device_, context);

#ifdef VK_DEBUG_LAYERS
  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
  createInfo.pfnCallback = debugCallback;
  context->vkCreateDebugReportCallbackEXT(context->instance_, &createInfo, nullptr, &context->debugCallback_);
#endif

  CreateSurface(context->instance_, context->physicalDevice_, window, *context, &context->surface_);

  CreateSwapChain(context, window.width_, window.height_, swapChainImageCount);

  CreateSemaphores(context->device_, &context->swapChain_);
}

void render::contextDestroy(context_t* context)
{
  vkDestroySemaphore(context->device_, context->swapChain_.imageAcquired_, nullptr);
  vkDestroySemaphore(context->device_, context->swapChain_.renderingComplete_, nullptr);

  for (uint32_t i = 0; i < context->swapChain_.imageCount_; ++i)
  {
    vkDestroyFramebuffer(context->device_, context->swapChain_.frameBuffer_[i], nullptr);
    vkDestroyImageView(context->device_, context->swapChain_.imageView_[i], nullptr);
    vkDestroyFence(context->device_, context->swapChain_.frameFence_[i], nullptr);
  }

  //Destroy depthstencil buffer
  vkDestroyImageView(context->device_, context->swapChain_.depthStencil_.imageView_, nullptr);
  vkDestroyImage(context->device_, context->swapChain_.depthStencil_.image_, nullptr);
  gpuMemoryDeallocate(*context, context->swapChain_.depthStencil_.memory_);

  vkDestroyCommandPool(context->device_, context->commandPool_, nullptr);
  vkDestroyRenderPass(context->device_, context->swapChain_.renderPass_, nullptr);
  vkDestroySwapchainKHR(context->device_, context->swapChain_.handle_, nullptr);
  vkDestroySurfaceKHR(context->instance_, context->surface_.handle_, nullptr);

#ifdef VK_DEBUG_LAYERS
  context->vkDestroyDebugReportCallbackEXT(context->instance_, context->debugCallback_, nullptr);
#endif

  vkDestroyDevice(context->device_, nullptr);
  vkDestroyInstance(context->instance_, nullptr);
}


void render::swapchainResize(context_t* context, uint32_t width, uint32_t height)
{
  //TODO: Handle width and height equal 0!
  contextFlush(*context);

  //Destroy framebuffers
  for (uint32_t i = 0; i < context->swapChain_.imageCount_; ++i)
  {
    vkDestroyFramebuffer(context->device_, context->swapChain_.frameBuffer_[i], nullptr);
    vkDestroyImageView(context->device_, context->swapChain_.imageView_[i], nullptr);
    vkDestroyFence(context->device_, context->swapChain_.frameFence_[i], nullptr);
  }

  vkFreeCommandBuffers(context->device_, context->commandPool_, context->swapChain_.imageCount_, context->swapChain_.commandBuffer_.data());

  //Destroy depthstencil buffer
  vkDestroyImageView(context->device_, context->swapChain_.depthStencil_.imageView_, nullptr);
  vkDestroyImage(context->device_, context->swapChain_.depthStencil_.image_, nullptr);
  gpuMemoryDeallocate(*context, context->swapChain_.depthStencil_.memory_);

  //Recreate swapchain with the new size
  vkDestroyRenderPass(context->device_, context->swapChain_.renderPass_, nullptr);
  vkDestroySwapchainKHR(context->device_, context->swapChain_.handle_, nullptr);
  CreateSwapChain(context, width, height, context->swapChain_.imageCount_);

}

void render::contextFlush(const context_t& context)
{
  vkWaitForFences(context.device_, context.swapChain_.imageCount_, context.swapChain_.frameFence_.data(), VK_TRUE, UINT64_MAX);
  vkQueueWaitIdle(context.graphicsQueue_.handle_);
  vkQueueWaitIdle(context.computeQueue_.handle_);
}

VkCommandBuffer render::beginPresentationCommandBuffer(const context_t& context, uint32_t index, VkClearValue* clearValues)
{
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

  renderPassBeginInfo.renderArea.extent = { context.swapChain_.imageWidth_,context.swapChain_.imageHeight_ };
  renderPassBeginInfo.renderPass = context.swapChain_.renderPass_;

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

  //Begin command buffer
  vkBeginCommandBuffer(context.swapChain_.commandBuffer_[index], &beginInfo);

  //Begin render pass
  renderPassBeginInfo.framebuffer = context.swapChain_.frameBuffer_[index];
  vkCmdBeginRenderPass(context.swapChain_.commandBuffer_[index], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

  //Set viewport and scissor rectangle
  VkViewport viewPort = { 0.0f, 0.0f, (float)context.swapChain_.imageWidth_, (float)context.swapChain_.imageHeight_, 0.0f, 1.0f };
  VkRect2D scissorRect = { {0,0},{context.swapChain_.imageWidth_,context.swapChain_.imageHeight_} };
  vkCmdSetViewport(context.swapChain_.commandBuffer_[index], 0, 1, &viewPort);
  vkCmdSetScissor(context.swapChain_.commandBuffer_[index], 0, 1, &scissorRect);

  return context.swapChain_.commandBuffer_[index];
}

void render::endPresentationCommandBuffer(const context_t& context, uint32_t index)
{
  vkCmdEndRenderPass(context.swapChain_.commandBuffer_[index]);
  vkEndCommandBuffer(context.swapChain_.commandBuffer_[index]);
}

void render::presentNextImage(context_t* context, VkSemaphore* waitSemaphore, uint32_t waitSemaphoreCount)
{
  //Aquire next image in the swapchain
  context->vkAcquireNextImageKHR(context->device_,
    context->swapChain_.handle_,
    UINT64_MAX, context->swapChain_.imageAcquired_,
    VK_NULL_HANDLE, &context->swapChain_.currentImage_);

  uint32_t currentImage = context->swapChain_.currentImage_;

  //Submit current command buffer
  std::vector<VkSemaphore> waitSemaphoreList(1 + waitSemaphoreCount);
  std::vector<VkPipelineStageFlags> waitStageList(1 + waitSemaphoreCount);
  waitSemaphoreList[0] = context->swapChain_.imageAcquired_;
  waitStageList[0] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  for (size_t i(0); i < waitSemaphoreCount; ++i)
  {
    waitSemaphoreList[i+1] = waitSemaphore[i];
    waitStageList[i+1] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  }
  VkSubmitInfo submitInfo = {};
  
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphoreList.size();
  submitInfo.pWaitSemaphores = waitSemaphoreList.data();	      //Wait until image is aquired
  submitInfo.signalSemaphoreCount = 1u;
  submitInfo.pSignalSemaphores = &context->swapChain_.renderingComplete_;	//When command buffer has finished will signal renderingCompleteSemaphore
  submitInfo.pWaitDstStageMask = waitStageList.data();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &context->swapChain_.commandBuffer_[currentImage];
  vkQueueSubmit(context->graphicsQueue_.handle_, 1, &submitInfo, VK_NULL_HANDLE);

  //Present the image
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &context->swapChain_.renderingComplete_;	//Wait until rendering has finished
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &context->swapChain_.handle_;
  presentInfo.pImageIndices = &currentImage;
  context->vkQueuePresentKHR(context->graphicsQueue_.handle_, &presentInfo);

  //Submit presentation
  vkResetFences(context->device_, 1, &context->swapChain_.frameFence_[currentImage]);
  vkQueueSubmit(context->graphicsQueue_.handle_, 0, nullptr, context->swapChain_.frameFence_[currentImage]);
  vkWaitForFences(context->device_, 1, &context->swapChain_.frameFence_[currentImage], VK_TRUE, UINT64_MAX);
}

bool render::shaderCreateFromSPIRV(const context_t& context, shader_t::type type, const char* file, shader_t* shader)
{
  shader->handle_ = VK_NULL_HANDLE;
  shader->type_ = type;

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
  VkResult result = vkCreateShaderModule(context.device_, &shaderCreateInfo, NULL, &shader->handle_);
  delete[] code;

  return result == VK_SUCCESS;
}

bool render::shaderCreateFromGLSL(const context_t& context, shader_t::type type, const char* file, shader_t* shader)
{
  std::string spirv_file_path = "temp.spv";
  std::string glslangvalidator_params = "arg0 -V -o \"" + spirv_file_path + "\" \"" + file + "\"";

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

bool render::shaderCreateFromGLSLSource(const context_t& context, shader_t::type type, const char* glslSource, shader_t* shader)
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
  vkDestroyShaderModule(context.device_, shader->handle_, nullptr);
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

    for (uint32_t i = 0; i < context.memoryProperties_.memoryTypeCount; i++)
    {
      if ((CHECK_BIT(memoryTypes, i)) && ((context.memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties))
      {
        //Try allocating the memory
        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.memoryTypeIndex = i;
        memoryAllocateInfo.allocationSize = size;
        VkDeviceMemory memory;
        if (vkAllocateMemory(context.device_, &memoryAllocateInfo, nullptr, &memory) == VK_SUCCESS)
        {
          result.handle_ = memory;
          result.size_ = size;
          return result;
        }
      }
    }
  }
  else
  {
    VkDeviceSize offset = GetNextMultiple(allocator->head_, alignment);
    if (size <= allocator->size_ - offset)
    {
      result.handle_ = allocator->memory_;
      result.size_ = size;
      result.offset_ = offset;
      allocator->head_ = offset + size;
    }
  }

  return result;
}

void render::gpuMemoryDeallocate(const context_t& context, gpu_memory_t memory, gpu_memory_allocator_t* allocator)
{
  if (allocator == nullptr)
  {
    vkFreeMemory(context.device_, memory.handle_, nullptr);
  }
  else
  {
    // TODO
  }
}

void* render::gpuMemoryMap(const context_t& context, gpu_memory_t memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
  if (size == VK_WHOLE_SIZE)
  {
    size = memory.size_ - offset;
  }

  void* result = nullptr;
  vkMapMemory(context.device_, memory.handle_, memory.offset_ + offset, size, flags, &result);
  return result;
}

void render::gpuMemoryUnmap(const context_t& context, gpu_memory_t memory)
{
  vkUnmapMemory(context.device_, memory.handle_);
}


void render::gpuAllocatorCreate(const context_t& context, size_t size,
  uint32_t memoryTypes, uint32_t flags,
  gpu_memory_allocator_t* allocator)
{
  gpu_memory_t memory = gpuMemoryAllocate(context, size, 0u, memoryTypes, flags);
  allocator->memory_ = memory.handle_;
  allocator->size_ = size;
  allocator->head_ = 0;
}

void render::gpuAllocatorDestroy(const context_t& context, gpu_memory_allocator_t* allocator)
{
  vkFreeMemory(context.device_, allocator->memory_, nullptr);
}

void render::texture2DCreate(const context_t& context, const image::image2D_t* images, uint32_t imageCount, texture_sampler_t sampler, texture_t* texture)
{
  //Get base level image width and height
  VkExtent3D extents = { images[0].width_, images[0].height_, 1u };

  VkFormat format = VK_FORMAT_UNDEFINED;
  if(images[0].componentCount_ == 3)
  {
    format = VK_FORMAT_R8G8B8_UNORM;
  }
  else if (images[0].componentCount_ == 4)
  {
    format = VK_FORMAT_R8G8B8A8_UNORM;
  }

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
  vkCreateImage(context.device_, &imageCreateInfo, nullptr, &texture->image_);


  //Allocate and bind memory for the image.
  //note: Memory for the image is not host visible so we will need a host visible buffer to transfer the data
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device_, texture->image_, &requirements);
  texture->memory_ = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context.device_, texture->image_, texture->memory_.handle_, texture->memory_.offset_);

  //Upload data to the texture using an staging buffer

  //Create a staging buffer and memory store for the buffer
  VkBuffer stagingBuffer;


  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.pNext = nullptr;
  bufferCreateInfo.size = requirements.size;
  bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(context.device_, &bufferCreateInfo, nullptr, &stagingBuffer);

  //Allocate and bind memory to the buffer
  vkGetBufferMemoryRequirements(context.device_, stagingBuffer, &requirements);
  gpu_memory_t stagingBufferMemory;
  stagingBufferMemory = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, HOST_VISIBLE);
  vkBindBufferMemory(context.device_, stagingBuffer, stagingBufferMemory.handle_, stagingBufferMemory.offset_);

  //Map buffer memory and copy image data
  unsigned char* mapping = (unsigned char*)gpuMemoryMap(context, stagingBufferMemory);
  if (mapping)
  {
    for (uint32_t i(0); i < imageCount; ++i)
    {
      memcpy(mapping, images[i].data_, images[i].dataSize_);
      mapping += images[i].dataSize_;
    }

    gpuMemoryUnmap(context, stagingBufferMemory);
  }

  //Create command buffer
  VkCommandBuffer uploadCommandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool_;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context.device_, &commandBufferAllocateInfo, &uploadCommandBuffer);

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
  imageBarrier.image = texture->image_;
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
    texture->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
  vkCreateFence(context.device_, &fenceCreateInfo, nullptr, &fence);
  vkResetFences(context.device_, 1u, &fence);
  vkQueueSubmit(context.graphicsQueue_.handle_, 1, &submitInfo, fence);

  //Destroy temporary resources
  vkWaitForFences(context.device_, 1u, &fence, VK_TRUE, UINT64_MAX);
  vkDestroyFence(context.device_, fence, nullptr);
  vkFreeCommandBuffers(context.device_, context.commandPool_, 1, &uploadCommandBuffer);
  gpuMemoryDeallocate(context, stagingBufferMemory);
  vkDestroyBuffer(context.device_, stagingBuffer, nullptr);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.format = imageCreateInfo.format;
  imageViewCreateInfo.image = texture->image_;
  imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vkCreateImageView(context.device_, &imageViewCreateInfo, nullptr, &texture->imageView_);

  //Create sampler
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)sampler.magnification_;
  samplerCreateInfo.minFilter = (VkFilter)sampler.minification_;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)sampler.mipmap_;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)sampler.wrapU_;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)sampler.wrapV_;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)sampler.wrapW_;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = imageCount - 1.0f;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device_, &samplerCreateInfo, nullptr, &texture->sampler_);

  
  texture->descriptor_ = {};
  texture->descriptor_.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  texture->descriptor_.imageView = texture->imageView_;
  texture->descriptor_.sampler = texture->sampler_;
  texture->layout_ = VK_IMAGE_LAYOUT_GENERAL;
  texture->extent_ = extents;
  texture->mipLevels_ = 1;
  texture->aspectFlags_ = VK_IMAGE_ASPECT_COLOR_BIT;
  texture->format_ = format;

  textureChangeLayoutNow(context, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, texture);
}

void render::texture2DCreate(const context_t& context,
  uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
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
  imageCreateInfo.mipLevels = 1;
  imageCreateInfo.format = format;
  imageCreateInfo.arrayLayers = 1;
  imageCreateInfo.extent = extents;
  imageCreateInfo.usage = usage;
  imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
  imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  vkCreateImage(context.device_, &imageCreateInfo, nullptr, &texture->image_);

  //Allocate and bind memory for the image.
  //note: Memory for the image is not host visible so we will need a host visible buffer to transfer the data
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(context.device_, texture->image_, &requirements);
  texture->memory_ = gpuMemoryAllocate(context, requirements.size, requirements.alignment, requirements.memoryTypeBits, DEVICE_LOCAL);
  vkBindImageMemory(context.device_, texture->image_, texture->memory_.handle_, 0);

  //Create imageview
  VkImageViewCreateInfo imageViewCreateInfo = {};
  imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  imageViewCreateInfo.format = imageCreateInfo.format;
  imageViewCreateInfo.image = texture->image_;
  imageViewCreateInfo.subresourceRange.aspectMask = aspectFlags;
  imageViewCreateInfo.subresourceRange.levelCount = 1;
  imageViewCreateInfo.subresourceRange.layerCount = 1;
  imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vkCreateImageView(context.device_, &imageViewCreateInfo, nullptr, &texture->imageView_);

  //Create sampler
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)sampler.magnification_;
  samplerCreateInfo.minFilter = (VkFilter)sampler.minification_;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)sampler.mipmap_;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)sampler.wrapU_;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)sampler.wrapV_;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)sampler.wrapW_;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = 0.0;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device_, &samplerCreateInfo, nullptr, &texture->sampler_);

  texture->descriptor_ = {};
  texture->descriptor_.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->descriptor_.imageView = texture->imageView_;
  texture->descriptor_.sampler = texture->sampler_;
  texture->layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
  texture->mipLevels_ = 1;
  texture->aspectFlags_ = aspectFlags;
  texture->format_ = format;
}

void render::textureDestroy(const context_t& context, texture_t* texture)
{
  vkDestroyImageView(context.device_, texture->imageView_, nullptr);
  vkDestroyImage(context.device_, texture->image_, nullptr);
  vkDestroySampler(context.device_, texture->sampler_, nullptr);
  gpuMemoryDeallocate(context, texture->memory_);
}

void render::textureChangeLayout(const context_t& context, VkCommandBuffer cmdBuffer, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, texture_t* texture)
{
    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.pNext = nullptr;
    imageMemoryBarrier.oldLayout = texture->layout_;
    imageMemoryBarrier.newLayout = newLayout;
    imageMemoryBarrier.image = texture->image_;
    imageMemoryBarrier.subresourceRange.levelCount = 1u;
    imageMemoryBarrier.subresourceRange.layerCount = 1u;
    imageMemoryBarrier.subresourceRange.aspectMask = texture->aspectFlags_;

    // Source layouts (old)
    // Source access mask controls actions that have to be finished on the old layout
    // before it will be transitioned to the new layout
    switch (texture->layout_)
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
      cmdBuffer,
      srcStageMask,
      dstStageMask,
      0,
      0, nullptr,
      0, nullptr,
      1, &imageMemoryBarrier);
  

  texture->layout_ = newLayout;
  texture->descriptor_.imageLayout = newLayout;
}


void render::textureChangeLayoutNow(const context_t& context, VkImageLayout layout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, texture_t* texture)
{
  if (layout == texture->layout_)
  {
    return;
  }

  //Create command buffer
  VkCommandBuffer commandBuffer;
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool_;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context.device_, &commandBufferAllocateInfo, &commandBuffer); 

  //Begin command buffer
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  textureChangeLayout(context, commandBuffer, layout, srcStageMask, dstStageMask, texture);
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
  vkCreateFence(context.device_, &fenceCreateInfo, nullptr, &fence);
  vkResetFences(context.device_, 1u, &fence);
  vkQueueSubmit(context.graphicsQueue_.handle_, 1, &submitInfo, fence);


  //Destroy command buffer
  vkWaitForFences(context.device_, 1u, &fence, VK_TRUE, UINT64_MAX);
  vkFreeCommandBuffers(context.device_, context.commandPool_, 1, &commandBuffer);
  vkDestroyFence(context.device_, fence, nullptr);
}



void render::gpuBufferCreate(const context_t& context,
  gpu_buffer_t::usage usage, uint32_t memoryType, void* data,
  size_t size, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator)
{
  //Create the buffer
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = static_cast<uint32_t> (size);
  bufferCreateInfo.usage = (VkBufferUsageFlagBits)usage;
  vkCreateBuffer(context.device_, &bufferCreateInfo, nullptr, &buffer->handle_);

  //Get memory requirements of the buffer
  VkMemoryRequirements requirements = {};
  vkGetBufferMemoryRequirements(context.device_, buffer->handle_, &requirements);

  //Allocate memory for the buffer
  buffer->memory_ = gpuMemoryAllocate(context, requirements.size, requirements.alignment, 0xFFFF, memoryType, allocator);

  //Bind memory to the buffer
  vkBindBufferMemory(context.device_, buffer->handle_, buffer->memory_.handle_, buffer->memory_.offset_);

  if (data)
  {
    //Fill the buffer
    void* mapping = gpuMemoryMap(context, buffer->memory_);
    assert(mapping);
    memcpy(mapping, data, size);
    gpuMemoryUnmap(context, buffer->memory_);
  }

  //Initialize descriptor
  buffer->descriptor_ = {};
  buffer->descriptor_.offset = 0;
  buffer->descriptor_.buffer = buffer->handle_;
  buffer->descriptor_.range = size;

  buffer->usage_ = usage;
}

void render::gpuBufferCreate(const context_t& context, gpu_buffer_t::usage usage, void* data, size_t size, gpu_memory_allocator_t* allocator, gpu_buffer_t* buffer)
{
  return gpuBufferCreate(context, usage, HOST_VISIBLE, data, size, buffer, allocator);
}

void render::gpuBufferDestroy(const context_t& context, gpu_buffer_t* buffer, gpu_memory_allocator_t* allocator)
{
  vkDestroyBuffer(context.device_, buffer->handle_, nullptr);
  gpuMemoryDeallocate(context, buffer->memory_, allocator);
}

void render::gpuBufferUpdate(const context_t& context, void* data, size_t offset, size_t size, gpu_buffer_t* buffer)
{
  void* mapping = gpuMemoryMap(context, buffer->memory_, offset, size);
  assert(mapping);
  memcpy(mapping, data, size);
  gpuMemoryUnmap(context, buffer->memory_);
}

void* render::gpuBufferMap(const context_t& context, const gpu_buffer_t& buffer)
{
  return gpuMemoryMap(context, buffer.memory_, buffer.memory_.offset_, buffer.memory_.size_);
}

void render::gpuBufferUnmap(const context_t& context, const gpu_buffer_t& buffer)
{
  gpuMemoryUnmap(context, buffer.memory_);
}

descriptor_t render::getDescriptor(const gpu_buffer_t& buffer)
{
  descriptor_t descriptor;
  descriptor.bufferDescriptor_ = buffer.descriptor_;
  return descriptor;
}

descriptor_t render::getDescriptor(const texture_t& texture)
{
  descriptor_t descriptor;
  descriptor.imageDescriptor_ = texture.descriptor_;
  return descriptor;
}

descriptor_t render::getDescriptor(const depth_stencil_buffer_t& depthStencilBuffer)
{
  descriptor_t descriptor;
  descriptor.imageDescriptor_ = depthStencilBuffer.descriptor_;
  return descriptor;
}


void render::descriptorSetLayoutCreate(const context_t& context, descriptor_binding_t* bindings, uint32_t bindingCount, descriptor_set_layout_t* descriptorSetLayout)
{
  descriptorSetLayout->bindingCount_ = bindingCount;
  descriptorSetLayout->bindings_ = nullptr;
  descriptorSetLayout->handle_ = VK_NULL_HANDLE;

  if (bindingCount > 0)
  {
    descriptorSetLayout->bindings_ = new descriptor_binding_t[bindingCount];
    memcpy(descriptorSetLayout->bindings_, bindings, sizeof(descriptor_binding_t)*bindingCount);
  }
  
  std::vector<VkDescriptorSetLayoutBinding> layoutBindings(bindingCount);
  for (uint32_t i(0); i < layoutBindings.size(); ++i)
  {
    layoutBindings[i].descriptorCount = 1;
    layoutBindings[i].descriptorType = (VkDescriptorType)descriptorSetLayout->bindings_[i].type_;
    layoutBindings[i].binding = descriptorSetLayout->bindings_[i].binding_;
    layoutBindings[i].stageFlags = descriptorSetLayout->bindings_[i].stageFlags_;
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

  vkCreateDescriptorSetLayout(context.device_, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout->handle_);  
}

void render::descriptorSetLayoutDestroy(const context_t& context, descriptor_set_layout_t* desriptorSetLayout)
{
  delete[] desriptorSetLayout->bindings_;
  vkDestroyDescriptorSetLayout(context.device_, desriptorSetLayout->handle_, nullptr);
}

void render::pipelineLayoutCreate(const context_t& context, descriptor_set_layout_t* descriptorSetLayouts, uint32_t descriptorSetLayoutCount, pipeline_layout_t* pipelineLayout)
{
  //Create pipeline layout
  pipelineLayout->descriptorSetLayoutCount_ = descriptorSetLayoutCount;
  pipelineLayout->descriptorSetLayout_ = nullptr;
  pipelineLayout->handle_ = VK_NULL_HANDLE;

  if (descriptorSetLayoutCount > 0)
  {
    pipelineLayout->descriptorSetLayout_ = new descriptor_set_layout_t[descriptorSetLayoutCount];
    memcpy(pipelineLayout->descriptorSetLayout_, descriptorSetLayouts, sizeof(descriptor_set_layout_t)*descriptorSetLayoutCount);
  }

  std::vector<VkDescriptorSetLayout> setLayouts(descriptorSetLayoutCount);
  for (uint32_t i(0); i < descriptorSetLayoutCount; ++i)
  {
    setLayouts[i] = pipelineLayout->descriptorSetLayout_[i].handle_;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutCount;
  pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
  vkCreatePipelineLayout(context.device_, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout->handle_);
}

void render::pipelineLayoutDestroy(const context_t& context, pipeline_layout_t* pipelineLayout)
{
  delete[] pipelineLayout->descriptorSetLayout_;
  vkDestroyPipelineLayout(context.device_, pipelineLayout->handle_, nullptr);
}

void render::descriptorPoolCreate(const context_t& context, uint32_t descriptorSetsCount, 
                                                            uint32_t combinedImageSamplersCount, uint32_t uniformBuffersCount, uint32_t storageBuffersCount, uint32_t storageImagesCount, 
                                                            descriptor_pool_t* descriptorPool)
{
  descriptorPool->descriptorSets_ = descriptorSetsCount;
  descriptorPool->combinedImageSamplers_ = combinedImageSamplersCount;
  descriptorPool->uniformBuffers_ = uniformBuffersCount;
  descriptorPool->storageBuffers_ = storageBuffersCount;
  descriptorPool->storageImages_ = storageImagesCount;

  std::vector<VkDescriptorPoolSize> descriptorPoolSize;
  descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorPool->combinedImageSamplers_ });
  descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorPool->uniformBuffers_ });
  descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorPool->storageBuffers_ });
  descriptorPoolSize.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descriptorPool->storageImages_ });


  //Create DescriptorPool
  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolCreateInfo.maxSets = descriptorPool->descriptorSets_;
  descriptorPoolCreateInfo.poolSizeCount = (uint32_t)descriptorPoolSize.size();
  descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize.data();

  vkCreateDescriptorPool(context.device_, &descriptorPoolCreateInfo, nullptr, &descriptorPool->handle_);
}

void render::descriptorPoolDestroy(const context_t& context, descriptor_pool_t* descriptorPool)
{
  vkDestroyDescriptorPool(context.device_, descriptorPool->handle_, nullptr);
}

void render::descriptorSetCreate(const context_t& context, const descriptor_pool_t& descriptorPool, const descriptor_set_layout_t& descriptorSetLayout, descriptor_t* descriptors, descriptor_set_t* descriptorSet)
{
  descriptorSet->descriptorCount_ = descriptorSetLayout.bindingCount_;
  descriptorSet->descriptors_ = new descriptor_t[descriptorSet->descriptorCount_];
  memcpy(descriptorSet->descriptors_, descriptors, sizeof(descriptor_t)*descriptorSet->descriptorCount_);
  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
  descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout.handle_;
  descriptorSetAllocateInfo.descriptorSetCount = 1;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool.handle_;
  vkAllocateDescriptorSets(context.device_, &descriptorSetAllocateInfo, &descriptorSet->handle_);
  descriptorSetUpdate(context, descriptorSetLayout, descriptorSet);

  descriptorSet->pool_ = descriptorPool;
}

void render::descriptorSetDestroy(const context_t& context, descriptor_set_t* descriptorSet)
{
  delete[] descriptorSet->descriptors_;
  vkFreeDescriptorSets(context.device_, descriptorSet->pool_.handle_, 1, &descriptorSet->handle_);
}

void render::descriptorSetUpdate(const context_t& context, const descriptor_set_layout_t& descriptorSetLayout, descriptor_set_t* descriptorSet)
{
  std::vector<VkWriteDescriptorSet> writeDescriptorSets(descriptorSet->descriptorCount_);
  for (uint32_t i(0); i < writeDescriptorSets.size(); ++i)
  {
    writeDescriptorSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSets[i].dstSet = descriptorSet->handle_;
    writeDescriptorSets[i].descriptorCount = 1;
    writeDescriptorSets[i].descriptorType = (VkDescriptorType)descriptorSetLayout.bindings_[i].type_;
    writeDescriptorSets[i].dstBinding = descriptorSetLayout.bindings_[i].binding_;

    switch (descriptorSetLayout.bindings_[i].type_)
    {
    case descriptor_t::type::SAMPLER:
    case descriptor_t::type::COMBINED_IMAGE_SAMPLER:
    case descriptor_t::type::SAMPLED_IMAGE:
    case descriptor_t::type::STORAGE_IMAGE:
    {
      writeDescriptorSets[i].pImageInfo = &descriptorSet->descriptors_[i].imageDescriptor_;
      break;
    }

    case descriptor_t::type::UNIFORM_TEXEL_BUFFER:
    case descriptor_t::type::STORAGE_TEXEL_BUFFER:
    case descriptor_t::type::UNIFORM_BUFFER:
    case descriptor_t::type::STORAGE_BUFFER:
    case descriptor_t::type::UNIFORM_BUFFER_DYNAMIC:
    case descriptor_t::type::STORAGE_BUFFER_DYNAMIC:
    case descriptor_t::type::INPUT_ATTACHMENT:
    {
      writeDescriptorSets[i].pBufferInfo = &descriptorSet->descriptors_[i].bufferDescriptor_;
      break;
    }
    }
  }

  vkUpdateDescriptorSets(context.device_, (uint32_t)writeDescriptorSets.size(), &writeDescriptorSets[0], 0, nullptr);
}

void render::descriptorSetBindForGraphics(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount)
{
  std::vector<VkDescriptorSet> descriptorSetHandles(descriptorSetCount);
  for (u32 i(0); i < descriptorSetCount; ++i)
  {
    descriptorSetHandles[i] = descriptorSets[i].handle_;
  }
  
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.handle_, firstSet, descriptorSetCount, descriptorSetHandles.data(), 0, 0);
}

void render::descriptorSetBindForCompute(VkCommandBuffer commandBuffer, const pipeline_layout_t& pipelineLayout, uint32_t firstSet, descriptor_set_t* descriptorSets, uint32_t descriptorSetCount)
{
  std::vector<VkDescriptorSet> descriptorSetHandles(descriptorSetCount);
  for (u32 i(0); i < descriptorSetCount; ++i)
  {
    descriptorSetHandles[i] = descriptorSets[i].handle_;
  }

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.handle_, firstSet, descriptorSetCount, descriptorSetHandles.data(), 0, 0);
}


void render::graphicsPipelineCreate(const context_t& context, VkRenderPass renderPass, uint32_t subpass, const render::vertex_format_t& vertexFormat, 
  const pipeline_layout_t& pipelineLayout, const graphics_pipeline_t::description_t& pipelineDesc, graphics_pipeline_t* pipeline)
{
  pipeline->desc_ = pipelineDesc;
  VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
  pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  pipelineViewportStateCreateInfo.viewportCount = 1;
  pipelineViewportStateCreateInfo.pViewports = &pipeline->desc_.viewPort_;
  pipelineViewportStateCreateInfo.scissorCount = 1;
  pipelineViewportStateCreateInfo.pScissors = &pipeline->desc_.scissorRect_;

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
  pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  pipelineColorBlendStateCreateInfo.attachmentCount = (uint32_t)pipeline->desc_.blendState_.size();
  pipelineColorBlendStateCreateInfo.pAttachments = &pipeline->desc_.blendState_[0];

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
  pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
  pipelineRasterizationStateCreateInfo.cullMode = pipeline->desc_.cullMode_;
  pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
  pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
  pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  pipelineDepthStencilStateCreateInfo.depthTestEnable = pipeline->desc_.depthTestEnabled_;
  pipelineDepthStencilStateCreateInfo.depthWriteEnable = pipeline->desc_.depthWriteEnabled_;
  pipelineDepthStencilStateCreateInfo.depthCompareOp = pipeline->desc_.depthTestFunction_;;
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
  pipelineShaderStageCreateInfos[0].module = pipeline->desc_.vertexShader_.handle_;
  pipelineShaderStageCreateInfos[0].pName = "main";
  pipelineShaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

  pipelineShaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  pipelineShaderStageCreateInfos[1].module = pipeline->desc_.fragmentShader_.handle_;
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

  graphicsPipelineCreateInfo.layout = pipelineLayout.handle_;
  graphicsPipelineCreateInfo.pVertexInputState = &vertexFormat.vertexInputState_;
  graphicsPipelineCreateInfo.pInputAssemblyState = &vertexFormat.inputAssemblyState_;
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
  vkCreateGraphicsPipelines(context.device_, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &pipeline->handle_);
}

void render::graphicsPipelineDestroy(const context_t& context, graphics_pipeline_t* pipeline)
{
  vkDestroyPipeline(context.device_, pipeline->handle_, nullptr);
}

void render::graphicsPipelineBind(VkCommandBuffer commandBuffer, const graphics_pipeline_t& pipeline)
{
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle_);
}


void render::computePipelineCreate(const context_t& context, const pipeline_layout_t& layout, compute_pipeline_t* pipeline)
{
  //Compute pipeline
  VkPipelineShaderStageCreateInfo shaderStage = {};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.pName = "main";
  shaderStage.module = pipeline->computeShader_.handle_;

  VkComputePipelineCreateInfo computePipelineCreateInfo = {};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.layout = layout.handle_;
  computePipelineCreateInfo.flags = 0;
  computePipelineCreateInfo.stage = shaderStage;
  vkCreateComputePipelines(context.device_, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline->handle_);
}

void render::computePipelineDestroy(const context_t& context, compute_pipeline_t* pipeline)
{
  vkDestroyPipeline(context.device_, pipeline->handle_, nullptr);
}

void render::computePipelineBind( VkCommandBuffer commandBuffer, const compute_pipeline_t& pipeline)
{
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.handle_);
}

static const VkFormat AttributeFormatLUT[] = { VK_FORMAT_R32_SINT, VK_FORMAT_R32_UINT, VK_FORMAT_R32_SFLOAT,
                                               VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_SFLOAT,
                                               VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SFLOAT,
                                               VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SFLOAT 
                                             };

static const uint32_t AttributeFormatSizeLUT[] = { 4u, 4u, 4u, 
                                                   8u, 8u, 8u, 
                                                   12u, 12u, 12u, 
                                                   16u, 16u, 16u
                                                 };

void render::vertexFormatCreate(vertex_attribute_t* attribute, uint32_t attributeCount, vertex_format_t* format)
{
  format->vertexSize_ = 0u;
  VkVertexInputAttributeDescription* attributeDescription = new VkVertexInputAttributeDescription[attributeCount];;
  VkVertexInputBindingDescription* bindingDescription = new VkVertexInputBindingDescription[attributeCount];
  for (uint32_t i = 0; i < attributeCount; ++i)
  {
    VkFormat attributeFormat = AttributeFormatLUT[attribute[i].format_];
    bindingDescription[i].binding = i;
    bindingDescription[i].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescription[i].stride = (uint32_t)attribute[i].stride_;
    attributeDescription[i].binding = i;
    attributeDescription[i].format = attributeFormat;
    attributeDescription[i].location = i;
    attributeDescription[i].offset = attribute[i].offset_;
    format->vertexSize_ += AttributeFormatSizeLUT[attribute[i].format_];
  }

  format->vertexInputState_ = {};
  format->vertexInputState_.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  format->vertexInputState_.vertexAttributeDescriptionCount = attributeCount;
  format->vertexInputState_.pVertexAttributeDescriptions = attributeDescription;
  format->vertexInputState_.vertexBindingDescriptionCount = attributeCount;
  format->vertexInputState_.pVertexBindingDescriptions = bindingDescription;

  format->inputAssemblyState_ = {};
  format->inputAssemblyState_.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  format->inputAssemblyState_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  format->attributeCount_ = attributeCount;
}

void render::vertexFormatDestroy(vertex_format_t* format)
{
  delete[] format->vertexInputState_.pVertexAttributeDescriptions;
  delete[] format->vertexInputState_.pVertexBindingDescriptions;
}



void render::depthStencilBufferCreate(const context_t& context, uint32_t width, uint32_t height, depth_stencil_buffer_t* depthStencilBuffer)
{
  CreateDepthStencilBuffer(&context, width, height, context.swapChain_.depthStencil_.format_, depthStencilBuffer);
  
  
  //Create sampler
  texture_sampler_t defaultSampler;
  VkSampler sampler;
  VkSamplerCreateInfo samplerCreateInfo = {};
  samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerCreateInfo.magFilter = (VkFilter)defaultSampler.magnification_;
  samplerCreateInfo.minFilter = (VkFilter)defaultSampler.minification_;
  samplerCreateInfo.mipmapMode = (VkSamplerMipmapMode)defaultSampler.mipmap_;
  samplerCreateInfo.addressModeU = (VkSamplerAddressMode)defaultSampler.wrapU_;
  samplerCreateInfo.addressModeV = (VkSamplerAddressMode)defaultSampler.wrapV_;
  samplerCreateInfo.addressModeW = (VkSamplerAddressMode)defaultSampler.wrapW_;
  samplerCreateInfo.mipLodBias = 0.0f;
  samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
  samplerCreateInfo.minLod = 0.0f;
  samplerCreateInfo.maxLod = 0.0;
  samplerCreateInfo.maxAnisotropy = 1.0;
  vkCreateSampler(context.device_, &samplerCreateInfo, nullptr, &sampler);

  depthStencilBuffer->descriptor_.sampler = sampler;
  depthStencilBuffer->descriptor_.imageView = depthStencilBuffer->imageView_;
  depthStencilBuffer->descriptor_.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  depthStencilBuffer->aspectFlags_ = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  depthStencilBuffer->layout_ = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
}

void render::depthStencilBufferDestroy(const context_t& context, depth_stencil_buffer_t* depthStencilBuffer)
{
  vkDestroyImageView(context.device_, depthStencilBuffer->imageView_, nullptr);
  vkDestroyImage(context.device_, depthStencilBuffer->image_, nullptr);
  vkDestroySampler(context.device_, depthStencilBuffer->descriptor_.sampler, nullptr);
  gpuMemoryDeallocate(context, depthStencilBuffer->memory_);
}


void render::depthStencilBufferChangeLayout(const context_t& context, VkCommandBuffer cmdBuffer, VkImageLayout newLayout, depth_stencil_buffer_t* depthStencilBuffer)
{
  VkImageMemoryBarrier imageBarrier = {};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imageBarrier.pNext = nullptr;
  imageBarrier.oldLayout = depthStencilBuffer->layout_;
  imageBarrier.newLayout = newLayout;
  imageBarrier.image = depthStencilBuffer->image_;
  imageBarrier.subresourceRange.aspectMask = depthStencilBuffer->aspectFlags_;

  imageBarrier.subresourceRange.baseMipLevel = 0;
  imageBarrier.subresourceRange.levelCount = 0;
  imageBarrier.subresourceRange.baseArrayLayer = 0;
  imageBarrier.subresourceRange.layerCount = 1;
  imageBarrier.subresourceRange.layerCount = 1;
  imageBarrier.subresourceRange.levelCount = 1;

  switch (imageBarrier.oldLayout)
  {
  case VK_IMAGE_LAYOUT_PREINITIALIZED:
    imageBarrier.srcAccessMask =
      VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    break;
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    imageBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    break;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    imageBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    imageBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    break;
  }

  switch (imageBarrier.newLayout)
  {
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    break;
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    imageBarrier.srcAccessMask |= VK_ACCESS_TRANSFER_READ_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    break;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    imageBarrier.dstAccessMask |=
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    break;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    imageBarrier.srcAccessMask =
      VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    break;
  }



  vkCmdPipelineBarrier(cmdBuffer,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    0, 0, nullptr, 0, nullptr,
    1, &imageBarrier);


  depthStencilBuffer->layout_ = newLayout;
  depthStencilBuffer->descriptor_.imageLayout = newLayout;
}


void render::renderPassCreate(const context_t& context,
  render_pass_t::attachment_t* attachments, uint32_t attachmentCount,
  render_pass_t::subpass_t* subpasses, uint32_t subpassCount,
  render_pass_t::subpass_dependency_t* dependencies, uint32_t dependencyCount,
  render_pass_t* renderPass)
{
  renderPass->attachment_ = new render_pass_t::attachment_t[attachmentCount];
  renderPass->attachmentCount_ = attachmentCount;
  memcpy(renderPass->attachment_, attachments, sizeof(render_pass_t::attachment_t)*attachmentCount);

  std::vector<VkAttachmentDescription> attachmentDescription(attachmentCount);
  for (uint32_t i(0); i < attachmentCount; ++i)
  {
    attachmentDescription[i].samples = attachments[i].samples_;
    attachmentDescription[i].format = attachments[i].format_;
    attachmentDescription[i].loadOp = attachments[i].loadOp_;
    attachmentDescription[i].storeOp = attachments[i].storeOp_;
    attachmentDescription[i].initialLayout = attachments[i].initialLayout_;
    attachmentDescription[i].finalLayout = attachments[i].finallLayout_;
  }

  
  if (subpassCount == 0u)
  {
    //Create default subpass
    VkSubpassDescription subpassDescription = {};
    std::vector<VkAttachmentReference> attachmentColorReference;
    VkAttachmentReference depthStencilAttachmentReference;
    for (u32 i(0); i < attachmentDescription.size(); ++i)
    {
      if (attachmentDescription[i].format == context.swapChain_.depthStencil_.format_)
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


    vkCreateRenderPass(context.device_, &renderPassCreateInfo, nullptr, &renderPass->handle_);
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
      uint32_t inputAttachmentCount = (uint32_t)subpasses[i].inputAttachmentIndex_.size();
      inputAttachmentRef[i].resize(inputAttachmentCount);
      for (uint32_t j = 0; j < inputAttachmentCount; ++j)
      {
        inputAttachmentRef[i][j].attachment = subpasses[i].inputAttachmentIndex_[j];
        inputAttachmentRef[i][j].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      subpassDescription[i].inputAttachmentCount = inputAttachmentCount;
      subpassDescription[i].pInputAttachments = inputAttachmentRef[i].data();

      //Color attachments
      uint32_t colorAttachmentCount = (uint32_t)subpasses[i].colorAttachmentIndex_.size();
      colorAttachmentRef[i].resize(colorAttachmentCount);
      for (uint32_t j = 0; j < colorAttachmentCount; ++j)
      {
        colorAttachmentRef[i][j].attachment = subpasses[i].colorAttachmentIndex_[j];
        colorAttachmentRef[i][j].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }
      subpassDescription[i].colorAttachmentCount = colorAttachmentCount;
      subpassDescription[i].pColorAttachments = colorAttachmentRef[i].data();
      

      //Depth stencil attachment
      if (subpasses[i].depthStencilAttachmentIndex_ != -1)
      {        
        depthStencilAttachmentRef[i].attachment = subpasses[i].depthStencilAttachmentIndex_;
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
    }

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = attachmentCount;
    renderPassCreateInfo.subpassCount = (uint32_t)subpassDescription.size();
    renderPassCreateInfo.pSubpasses = subpassDescription.data();
    renderPassCreateInfo.pAttachments = attachmentDescription.data();
    renderPassCreateInfo.dependencyCount = dependencyCount;
    renderPassCreateInfo.pDependencies = subpassDependencies.data();


    vkCreateRenderPass(context.device_, &renderPassCreateInfo, nullptr, &renderPass->handle_);
  }  
}

void render::renderPassDestroy(const context_t& context, render_pass_t* renderPass)
{
  delete[] renderPass->attachment_;
  vkDestroyRenderPass(context.device_, renderPass->handle_, nullptr);
}

void render::frameBufferCreate(const context_t& context, uint32_t width, uint32_t height, const render_pass_t& renderPass, VkImageView* imageViews, frame_buffer_t* frameBuffer)
{ 
  VkFramebufferCreateInfo framebufferCreateInfo = {};
  framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferCreateInfo.attachmentCount = renderPass.attachmentCount_;
  framebufferCreateInfo.pAttachments = imageViews;
  framebufferCreateInfo.width = width;
  framebufferCreateInfo.height = height;
  framebufferCreateInfo.layers = 1;
  framebufferCreateInfo.renderPass = renderPass.handle_;

  vkCreateFramebuffer(context.device_, &framebufferCreateInfo, nullptr, &frameBuffer->handle_);

  frameBuffer->renderPass_ = renderPass;
  frameBuffer->width_ = width;
  frameBuffer->height_ = height;
}


void render::frameBufferDestroy(const context_t& context, frame_buffer_t* frameBuffer)
{
  vkDestroyFramebuffer(context.device_, frameBuffer->handle_, nullptr);
}


void render::commandBufferCreate(const context_t& context, VkCommandBufferLevel level, VkSemaphore* waitSemaphore, VkPipelineStageFlags* waitStages, uint32_t waitSemaphoreCount, VkSemaphore* signalSemaphore, uint32_t signalSemaphoreCount, command_buffer_t::type type, command_buffer_t* commandBuffer)
{
  commandBuffer->type_ = type;
  commandBuffer->waitSemaphore_ = nullptr;
  commandBuffer->signalSemaphore_ = nullptr;
  commandBuffer->waitStages_ = nullptr;

  commandBuffer->waitSemaphoreCount_ = waitSemaphoreCount;  
  if(waitSemaphoreCount > 0)
  {
    commandBuffer->waitSemaphore_ = new VkSemaphore[waitSemaphoreCount];
    memcpy(commandBuffer->waitSemaphore_, waitSemaphore, sizeof(VkSemaphore)*waitSemaphoreCount );
    commandBuffer->waitStages_ = new VkPipelineStageFlags[waitSemaphoreCount];
    memcpy(commandBuffer->waitStages_, waitStages, sizeof(VkPipelineStageFlags)*waitSemaphoreCount);
  }
  

  commandBuffer->signalSemaphoreCount_ = signalSemaphoreCount;
  if (signalSemaphoreCount > 0)
  {
    commandBuffer->signalSemaphore_ = new VkSemaphore[signalSemaphoreCount];
    memcpy(commandBuffer->signalSemaphore_, signalSemaphore, sizeof(VkSemaphore)*signalSemaphoreCount);

  }

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = context.commandPool_;
  commandBufferAllocateInfo.level = level;
  vkAllocateCommandBuffers(context.device_, &commandBufferAllocateInfo, &commandBuffer->handle_ );

  VkFenceCreateInfo fenceCreateInfo = {};
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  vkCreateFence(context.device_, &fenceCreateInfo, nullptr, &commandBuffer->fence_);
}

void render::commandBufferDestroy(const context_t& context, command_buffer_t* commandBuffer )
{
  delete[] commandBuffer->waitSemaphore_;
  delete[] commandBuffer->waitStages_;
  delete[] commandBuffer->signalSemaphore_;

  vkFreeCommandBuffers(context.device_, context.commandPool_, 1u, &commandBuffer->handle_ );
  vkDestroyFence(context.device_, commandBuffer->fence_, nullptr);
}

void render::commandBufferBegin(const context_t& context, const frame_buffer_t* frameBuffer, VkClearValue* clearValues, uint32_t clearValuesCount, const command_buffer_t& commandBuffer)
{
  vkWaitForFences(context.device_, 1u, &commandBuffer.fence_, VK_TRUE, UINT64_MAX);
  
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  
  //Begin command buffer
  vkBeginCommandBuffer(commandBuffer.handle_, &beginInfo);

  
  if (commandBuffer.type_ == command_buffer_t::GRAPHICS)
  {
    //Begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    renderPassBeginInfo.renderArea.extent = { frameBuffer->width_ , frameBuffer->height_ };
    renderPassBeginInfo.renderPass = frameBuffer->renderPass_.handle_;

    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.clearValueCount = clearValuesCount;

    //Begin render pass
    renderPassBeginInfo.framebuffer = frameBuffer->handle_;
    vkCmdBeginRenderPass(commandBuffer.handle_, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    //Set viewport and scissor rectangle
    VkViewport viewPort = { 0.0f, 0.0f, (float)frameBuffer->width_, (float)frameBuffer->height_, 0.0f, 1.0f };
    VkRect2D scissorRect = { { 0,0 },{ frameBuffer->width_, frameBuffer->height_ } };
    vkCmdSetViewport(commandBuffer.handle_, 0, 1, &viewPort);
    vkCmdSetScissor(commandBuffer.handle_, 0, 1, &scissorRect);
  }
}

void render::commandBufferNextSubpass(const command_buffer_t& commandBuffer)
{
  vkCmdNextSubpass(commandBuffer.handle_, VK_SUBPASS_CONTENTS_INLINE);
}

void render::commandBufferEnd( const command_buffer_t& commandBuffer)
{
  if (commandBuffer.type_ == command_buffer_t::GRAPHICS)
  {
    vkCmdEndRenderPass(commandBuffer.handle_);
  }

  vkEndCommandBuffer(commandBuffer.handle_);
}


void render::commandBufferSubmit(const context_t& context, const command_buffer_t& commandBuffer )
{ 
  vkWaitForFences(context.device_, 1u, &commandBuffer.fence_, VK_TRUE, UINT64_MAX);
  vkResetFences(context.device_, 1, &commandBuffer.fence_);

  //Submit current command buffer
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = commandBuffer.waitSemaphoreCount_;
  submitInfo.pWaitSemaphores = commandBuffer.waitSemaphore_;
  submitInfo.signalSemaphoreCount = commandBuffer.signalSemaphoreCount_;
  submitInfo.pSignalSemaphores = commandBuffer.signalSemaphore_;
  submitInfo.pWaitDstStageMask = commandBuffer.waitStages_;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer.handle_;

  if(commandBuffer.type_ == command_buffer_t::GRAPHICS)
  {
    vkQueueSubmit(context.graphicsQueue_.handle_, 1, &submitInfo, commandBuffer.fence_);
  }
  else
  {
    vkQueueSubmit(context.computeQueue_.handle_, 1, &submitInfo, commandBuffer.fence_);
  }
}
