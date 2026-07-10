#include "vulkanBackend.h"

#include <set>
#include <stdexcept>

VulkanContext vulkanContext{};

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};

static void initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *mode = glfwGetVideoMode(monitor);

  vulkanContext.window =
      glfwCreateWindow(mode->width, mode->height, "Spaceguy", nullptr, nullptr);
  // vulkanContext.window = glfwCreateWindow(mode->width, mode->height,
  // "Spaceguy", monitor, nullptr);

  // We need to code ESC to close or something.
  // glfwSetWindowMonitor(vulkanContext.window, monitor, 0, 0, mode->width,
  // mode->height, mode->refreshRate);
}

static void createInstance() {
  vk::ApplicationInfo appInfo{};
  appInfo.sType = vk::StructureType::eApplicationInfo;
  appInfo.pApplicationName = "Spaceguy";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char *> extensions;

  for (uint32_t i = 0; i < glfwExtensionCount; i++) {
    extensions.push_back(glfwExtensions[i]);
  }

#ifdef __APPLE__
  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

  vk::InstanceCreateInfo createInfo{};
  createInfo.sType = vk::StructureType::eInstanceCreateInfo;
  createInfo.pApplicationInfo = &appInfo;

#ifdef __APPLE__
  createInfo.flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  createInfo.enabledLayerCount = 0;

  vulkanContext.instance =
      vk::raii::Instance{vulkanContext.context, createInfo};
}

static void createSurface() {
  VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

  if (glfwCreateWindowSurface(*vulkanContext.instance, vulkanContext.window,
                              nullptr, &rawSurface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create the window surface");
  }

  vulkanContext.surface =
      vk::raii::SurfaceKHR{vulkanContext.instance, rawSurface};
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    const bool supportsGraphics =
        queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;

    const bool supportsCompute =
        queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT;

    if (supportsGraphics && supportsCompute) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, *vulkanContext.surface,
                                         &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }
  }

  return indices;
}

static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) {
  SwapchainSupportDetails details;

  details.capabilities = vk::PhysicalDevice{device}.getSurfaceCapabilitiesKHR(
      *vulkanContext.surface);

  details.formats =
      vk::PhysicalDevice{device}.getSurfaceFormatsKHR(*vulkanContext.surface);

  details.presentModes = vk::PhysicalDevice{device}.getSurfacePresentModesKHR(
      *vulkanContext.surface);

  return details;
}

static bool isDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = findQueueFamilies(device);

  bool swapchainAdequate = false;

  if (indices.isComplete()) {
    SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);

    swapchainAdequate = !swapchainSupport.formats.empty() &&
                        !swapchainSupport.presentModes.empty();
  }

  return indices.isComplete() && swapchainAdequate;
}

vk::SampleCountFlagBits
chooseUsableSampleCount(vk::SampleCountFlagBits preferredSampleCount) {
  vk::PhysicalDeviceProperties physicalDeviceProperties =
      vulkanContext.physicalDevice.getProperties();

  vk::SampleCountFlags counts =
      physicalDeviceProperties.limits.framebufferColorSampleCounts &
      physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  uint32_t preferred = static_cast<uint32_t>(preferredSampleCount);

  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e64) &&
      counts & vk::SampleCountFlagBits::e64)
    return vk::SampleCountFlagBits::e64;
  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e32) &&
      counts & vk::SampleCountFlagBits::e32)
    return vk::SampleCountFlagBits::e32;
  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e16) &&
      counts & vk::SampleCountFlagBits::e16)
    return vk::SampleCountFlagBits::e16;
  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e8) &&
      counts & vk::SampleCountFlagBits::e8)
    return vk::SampleCountFlagBits::e8;
  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e4) &&
      counts & vk::SampleCountFlagBits::e4)
    return vk::SampleCountFlagBits::e4;
  if (preferred >= static_cast<uint32_t>(vk::SampleCountFlagBits::e2) &&
      counts & vk::SampleCountFlagBits::e2)
    return vk::SampleCountFlagBits::e2;

  return vk::SampleCountFlagBits::e1;
}

static void pickPhysicalDevice() {
  std::vector<vk::raii::PhysicalDevice> devices =
      vk::raii::PhysicalDevices{vulkanContext.instance};

  if (devices.empty()) {
    throw std::runtime_error("failed to find a GPU with Vulkan support");
  }

  bool foundSuitableDevice = false;

  for (auto &candidate : devices) {
    if (isDeviceSuitable(*candidate)) {
      vulkanContext.physicalDevice = std::move(candidate);
      foundSuitableDevice = true;
      break;
    }
  }

  if (!foundSuitableDevice) {
    throw std::runtime_error("failed to find a suitable GPU");
  }
}

static void createLogicalDevice() {
  QueueFamilyIndices indices = findQueueFamilies(*vulkanContext.physicalDevice);

  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;

  for (uint32_t queueFamily : uniqueQueueFamilies) {
    queueCreateInfos.push_back(
        vk::DeviceQueueCreateInfo{.queueFamilyIndex = queueFamily,
                                  .queueCount = 1,
                                  .pQueuePriorities = &queuePriority});
  }

  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef __APPLE__
  deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

  vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{
      .dynamicRendering = vk::True};

  vk::DeviceCreateInfo createInfo{
      .pNext = &dynamicRenderingFeatures,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data()};

  vulkanContext.device =
      vk::raii::Device{vulkanContext.physicalDevice, createInfo};

  vulkanContext.graphicsQueue =
      vk::raii::Queue{vulkanContext.device, indices.graphicsFamily.value(), 0};
  vulkanContext.presentQueue =
      vk::raii::Queue{vulkanContext.device, indices.presentFamily.value(), 0};
}

static void createCommandPool() {
  QueueFamilyIndices queueFamilyIndices =
      findQueueFamilies(*vulkanContext.physicalDevice);

  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

  vulkanContext.commandPool =
      vk::raii::CommandPool{vulkanContext.device, poolInfo};
}

static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

static vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes) {
  for (const auto availablePresetMode : availablePresentModes) {
    if (availablePresetMode == vk::PresentModeKHR::eMailbox) {
      return availablePresetMode;
    }
  }

  return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D
chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }

  int width = 0;
  int height = 0;

  glfwGetFramebufferSize(vulkanContext.window, &width, &height);

  vk::Extent2D actualExtent{
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height),
  };

  actualExtent.width =
      std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actualExtent.height =
      std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);

  return actualExtent;
}

static void createSwapchain() {
  SwapchainSupportDetails swapchainSupport =
      querySwapchainSupport(*vulkanContext.physicalDevice);

  vk::SurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapchainSupport.formats);

  vk::PresentModeKHR presentMode =
      chooseSwapPresentMode(swapchainSupport.presentModes);

  vk::Extent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

  uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;

  if (swapchainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapchainSupport.capabilities.maxImageCount) {
    imageCount = swapchainSupport.capabilities.maxImageCount;
  }

  QueueFamilyIndices indices = findQueueFamilies(*vulkanContext.physicalDevice);

  std::array<uint32_t, 2> queueFamilyIndices = {
      indices.graphicsFamily.value(),
      indices.presentFamily.value(),
  };

  vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  if (swapchainSupport.capabilities.supportedUsageFlags &
      vk::ImageUsageFlagBits::eTransferDst) {
    imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
  } else {
    throw std::runtime_error(
        "swapchain does not support transfer destination images");
  }

  vk::SwapchainCreateInfoKHR createInfo{
      .surface = *vulkanContext.surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = imageUsage,
      .preTransform = swapchainSupport.capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = presentMode,
      .clipped = vk::True,
      .oldSwapchain = nullptr};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount =
        static_cast<uint32_t>(queueFamilyIndices.size());
    createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
  }

  vulkanContext.swapchain =
      vk::raii::SwapchainKHR{vulkanContext.device, createInfo};
  vulkanContext.swapchainImages = vulkanContext.swapchain.getImages();

  vulkanContext.swapchainImageFormat = surfaceFormat.format;
  vulkanContext.swapchainExtent = extent;
}

static void createImageViews() {
  vulkanContext.swapchainImageViews.clear();
  vulkanContext.swapchainImageViews.reserve(
      vulkanContext.swapchainImages.size());

  for (vk::Image image : vulkanContext.swapchainImages) {
    vk::ImageViewCreateInfo createInfo{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = vulkanContext.swapchainImageFormat,
        .components =
            vk::ComponentMapping{.r = vk::ComponentSwizzle::eIdentity,
                                 .g = vk::ComponentSwizzle::eIdentity,
                                 .b = vk::ComponentSwizzle::eIdentity,
                                 .a = vk::ComponentSwizzle::eIdentity},
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1}};

    vulkanContext.swapchainImageViews.emplace_back(vulkanContext.device,
                                                   createInfo);
  }
}

void setupVulkan() {
  initWindow();
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();

  createSwapchain();
  createImageViews();
}
