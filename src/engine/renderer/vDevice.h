#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <map> // IWYU pragma: keep

namespace {

std::vector<const char *> requiredDeviceExtension = {
    vk::KHRSwapchainExtensionName,
#ifdef __APPLE__
    "VK_KHR_portability_subset",
#endif
};

bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice) {
  // Check if the physicalDevice supports the Vulkan 1.3 API version
  bool supportsVulkan1_3 =
      physicalDevice.getProperties().apiVersion >= vk::ApiVersion13;

  // Check if any of the queue families support graphics operations
  auto queueFamilies = physicalDevice.getQueueFamilyProperties();
  bool supportsGraphics =
      std::ranges::any_of(queueFamilies, [](auto const &qfp) {
        return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
      });

  // Check if all required physicalDevice extensions are available
  auto availableDeviceExtensions =
      physicalDevice.enumerateDeviceExtensionProperties();

  bool supportsAllRequiredExtensions = std::ranges::all_of(
      requiredDeviceExtension,
      [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
        return std::ranges::any_of(
            availableDeviceExtensions,
            [requiredDeviceExtension](auto const &availableDeviceExtension) {
              return strcmp(availableDeviceExtension.extensionName,
                            requiredDeviceExtension) == 0;
            });
      });

  // Check if the physicalDevice supports the required features (shader draw
  // parameters, dynamic rendering and extended dynamic state)
  auto features = physicalDevice.template getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
      features.template get<vk::PhysicalDeviceVulkan11Features>()
          .shaderDrawParameters &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .dynamicRendering &&
      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
          .extendedDynamicState &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .synchronization2;

  // Return true if the physicalDevice meets all the criteria
  return supportsVulkan1_3 && supportsGraphics &&
         supportsAllRequiredExtensions && supportsRequiredFeatures;
}

void pickPhysicalDevice(const vk::raii::Instance &instance,
                        vk::raii::PhysicalDevice &physicalDevice) {
  std::vector<vk::raii::PhysicalDevice> physicalDevices =
      instance.enumeratePhysicalDevices();
  auto const devIter =
      std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) {
        return isDeviceSuitable(physicalDevice);
      });
  if (devIter == physicalDevices.end()) {
    throw std::runtime_error("failed to find a suitable GPU!");
  }
  physicalDevice = *devIter;
}

void createLogicalDevice(vk::raii::PhysicalDevice &physicalDevice,
                         vk::raii::Device &device,
                         vk::raii::Queue &graphicsQueue,
                         vk::raii::SurfaceKHR &surface, uint32_t &queueIndex) {
  // find the index of the first queue family that supports graphics
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice.getQueueFamilyProperties();

  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
       qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eGraphics) &&
        physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
      // found a queue family that supports both graphics and present
      queueIndex = qfpIndex;
      break;
    }
  }
  if (queueIndex == UINT32_MAX) {
    throw std::runtime_error(
        "Could not find a queue for graphics and present -> terminating");
  }

  // query for Vulkan 1.3 features
  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {
          {},                             // vk::PhysicalDeviceFeatures2
          {.shaderDrawParameters = true}, // vk::PhysicalDeviceVulkan11Features
          {.synchronization2 = true,
           .dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features
          {.extendedDynamicState =
               true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
      };

  // create a Device

  float queuePriority = 0.5f;
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = queueIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority};
  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount =
          static_cast<uint32_t>(requiredDeviceExtension.size()),
      .ppEnabledExtensionNames = requiredDeviceExtension.data()};

  device = vk::raii::Device(physicalDevice, deviceCreateInfo);
  graphicsQueue = vk::raii::Queue(device, queueIndex, 0);
}
} // namespace

namespace Renderer {
struct VDevice {
  vk::raii::PhysicalDevice physicalDevice = nullptr;
  vk::raii::Device device = nullptr;
  vk::PhysicalDeviceFeatures deviceFeatures;
  vk::raii::Queue graphicsQueue = nullptr;
  std::vector<const char *> requiredDeviceExtension = {
      vk::KHRSwapchainExtensionName};
  // get the first index into queueFamilyProperties which supports both graphics
  // and present
  uint32_t queueIndex = ~0;

  void pickAndCreate(const vk::raii::Instance &instance,
                     vk::raii::SurfaceKHR &surface) {
    pickPhysicalDevice(instance, physicalDevice);
    createLogicalDevice(physicalDevice, device, graphicsQueue, surface,
                        queueIndex);
  }
};
} // namespace Renderer
