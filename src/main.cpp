#define GLFW_INCLUDE_NONE
#define VULKAN_HPP_NO_CONSTRUCTORS

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <optional>
#include <utility>
#include <fstream>
#include <array>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cstring>

struct SwapchainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct BufferWithMemory
{
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
};

GLFWwindow *window = nullptr;
vk::raii::Context context;
vk::raii::Instance instance{nullptr};
vk::raii::SurfaceKHR surface{nullptr};
vk::raii::PhysicalDevice physicalDevice{nullptr};
vk::raii::Device device{nullptr};

vk::raii::Queue graphicsQueue{nullptr};
vk::raii::Queue presentQueue{nullptr};

vk::raii::SwapchainKHR swapchain{nullptr};
std::vector<vk::Image> swapchainImages;
std::vector<vk::raii::ImageView> swapchainImageViews;
vk::Format swapchainImageFormat;
vk::Extent2D swapchainExtent;

vk::raii::PipelineLayout pipelineLayout{nullptr};
vk::raii::Pipeline graphicsPipeline{nullptr};

vk::raii::CommandPool commandPool{nullptr};
std::vector<vk::raii::CommandBuffer> commandBuffers;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
std::vector<vk::raii::Fence> inFlightFences;
uint32_t currentFrame = 0;

std::vector<vk::raii::Buffer> uniformBuffers;
std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
std::vector<void *> uniformBuffersMapped;

vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device)
{
    SwapchainSupportDetails details;

    details.capabilities = vk::PhysicalDevice{device}.getSurfaceCapabilitiesKHR(*surface);

    details.formats =
        vk::PhysicalDevice{device}.getSurfaceFormatsKHR(*surface);

    details.presentModes =
        vk::PhysicalDevice{device}.getSurfacePresentModesKHR(*surface);

    return details;
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
    for (const auto availablePresetMode : availablePresentModes)
    {
        if (availablePresetMode == vk::PresentModeKHR::eMailbox)
        {
            return availablePresetMode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;

    glfwGetFramebufferSize(window, &width, &height);

    vk::Extent2D actualExtent{
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}

static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &queueFamilyCount,
        queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, *surface, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }
    }

    return indices;
}

vk::raii::ShaderModule createShaderModule(const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    return vk::raii::ShaderModule{device, createInfo};
}

void initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(800, 600, "Spaceguy", nullptr, nullptr);
}

void createInstance()
{
    vk::ApplicationInfo appInfo{};
    appInfo.sType = vk::StructureType::eApplicationInfo;
    appInfo.pApplicationName = "Spaceguy";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions;

    for (uint32_t i = 0; i < glfwExtensionCount; i++)
    {
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

    instance = vk::raii::Instance{context, createInfo};
}

void createSurface()
{
    VkSurfaceKHR rawSurface = VK_NULL_HANDLE;

    if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create the window surface");
    }

    surface = vk::raii::SurfaceKHR{instance, rawSurface};
}

bool isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool swapchainAdequate = false;

    if (indices.isComplete())
    {
        SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);

        swapchainAdequate =
            !swapchainSupport.formats.empty() &&
            !swapchainSupport.presentModes.empty();
    }

    return indices.isComplete() && swapchainAdequate;
}

void pickPhysicalDevice()
{
    std::vector<vk::raii::PhysicalDevice> devices =
        vk::raii::PhysicalDevices{instance};

    if (devices.empty())
    {
        throw std::runtime_error("failed to find a GPU with Vulkan support");
    }

    bool foundSuitableDevice = false;

    for (auto &candidate : devices)
    {
        if (isDeviceSuitable(*candidate))
        {
            physicalDevice = std::move(candidate);
            foundSuitableDevice = true;
            break;
        }
    }

    if (!foundSuitableDevice)
    {
        throw std::runtime_error("failed to find a suitable GPU");
    }
}

void createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(*physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()};

    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        queueCreateInfos.push_back(vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = queueFamily,
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

    device = vk::raii::Device{physicalDevice, createInfo};

    graphicsQueue = vk::raii::Queue{device, indices.graphicsFamily.value(), 0};
    presentQueue = vk::raii::Queue{device, indices.presentFamily.value(), 0};
}

void createGraphicsPipeline()
{
    auto vertShaderCode = readFile("shaders/triangle.vert.spv");
    auto fragShaderCode = readFile("shaders/triangle.frag.spv");

    vk::raii::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::raii::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = *vertShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = *fragShaderModule,
        .pName = "main"};

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vertShaderStageInfo,
        fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::DescriptorSetLayout setLayouts[] = {*descriptorSetLayout};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr};

    pipelineLayout = vk::raii::PipelineLayout{device, pipelineLayoutInfo};

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainImageFormat};

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout,
        .renderPass = nullptr,
        .subpass = 0};

    graphicsPipeline = vk::raii::Pipeline{
        device, nullptr, pipelineInfo};
}

void cleanup()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void createSwapchain()
{
    SwapchainSupportDetails swapchainSupport = querySwapchainSupport(*physicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);

    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);

    vk::Extent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;

    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
    {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    QueueFamilyIndices indices = findQueueFamilies(*physicalDevice);

    std::array<uint32_t, 2> queueFamilyIndices = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    vk::SwapchainCreateInfoKHR createInfo{
        .surface = *surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .preTransform = swapchainSupport.capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = vk::True,
        .oldSwapchain = nullptr};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount =
            static_cast<uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    }
    else
    {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    swapchain = vk::raii::SwapchainKHR{device, createInfo};
    swapchainImages = swapchain.getImages();

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void createImageViews()
{
    swapchainImageViews.clear();
    swapchainImageViews.reserve(swapchainImages.size());

    for (vk::Image image : swapchainImages)
    {
        vk::ImageViewCreateInfo createInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = swapchainImageFormat,
            .components = vk::ComponentMapping{
                .r = vk::ComponentSwizzle::eIdentity,
                .g = vk::ComponentSwizzle::eIdentity,
                .b = vk::ComponentSwizzle::eIdentity,
                .a = vk::ComponentSwizzle::eIdentity},
            .subresourceRange = vk::ImageSubresourceRange{.aspectMask = vk::ImageAspectFlagBits::eColor, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

        swapchainImageViews.emplace_back(device, createInfo);
    }
}

void createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(*physicalDevice);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value()};

    commandPool = vk::raii::CommandPool{device, poolInfo};
}

void createCommandBuffers()
{
    commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers{device, allocInfo};
}

void createSyncObjects()
{
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();

    imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled};

    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        renderFinishedSemaphores.emplace_back(device, semaphoreInfo);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        imageAvailableSemaphores.emplace_back(device, semaphoreInfo);
        inFlightFences.emplace_back(device, fenceInfo);
    }
}

void transitionImageLayout(
    vk::raii::CommandBuffer const &commandBuffer,
    vk::Image image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::PipelineStageFlags srcStage,
    vk::AccessFlags srcAccess,
    vk::PipelineStageFlags dstStage,
    vk::AccessFlags dstAccess)
{
    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1}};

    commandBuffer.pipelineBarrier(
        srcStage,
        dstStage,
        {},
        nullptr,
        nullptr,
        barrier);
}

void recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex)
{
    auto &commandBuffer = commandBuffers[frameIndex];

    commandBuffer.begin(vk::CommandBufferBeginInfo{});

    transitionImageLayout(
        commandBuffer,
        swapchainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTopOfPipe,
        {},
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::ClearValue clearColor{
        .color = vk::ClearColorValue{
            .float32 = std::array<float, 4>{0.02f, 0.02f, 0.04f, 1.0f}}};

    vk::RenderingAttachmentInfo colorAttachment{
        .imageView = *swapchainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingInfo renderingInfo{
        .renderArea = vk::Rect2D{
            .offset = vk::Offset2D{0, 0},
            .extent = swapchainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment};

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(swapchainExtent.width),
        .height = static_cast<float>(swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};

    vk::Rect2D scissor{
        .offset = vk::Offset2D{0, 0},
        .extent = swapchainExtent};

    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipelineLayout,
        0,
        *descriptorSets[frameIndex],
        nullptr);

    commandBuffer.draw(3, 1, 0, 0);

    commandBuffer.endRendering();

    transitionImageLayout(
        commandBuffer,
        swapchainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {});

    commandBuffer.end();
}

void updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();

    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};

    ubo.model =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.75f));

    ubo.view = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)); //?????

    ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height), 0.1f, 10.0f);

    ubo.proj[1][1] *= -1.0f;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void drawFrame()
{
    device.waitForFences(
        *inFlightFences[currentFrame], vk::True, UINT64_MAX);

    uint32_t imageIndex = swapchain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphores[currentFrame], nullptr).value;

    updateUniformBuffer(currentFrame);

    device.resetFences(*inFlightFences[currentFrame]);

    commandBuffers[currentFrame].reset();
    recordCommandBuffer(currentFrame, imageIndex);

    vk::Semaphore waitSemaphores[] = {
        *imageAvailableSemaphores[currentFrame]};

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore signalSemaphores[] = {
        *renderFinishedSemaphores[imageIndex]};

    vk::CommandBuffer commandBuffer = *commandBuffers[currentFrame];

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores};

    graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

    vk::SwapchainKHR swapchains[] = {*swapchain};

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex};

    presentQueue.presentKHR(presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        bool typeMatches = typeFilter & (1 << i);
        bool hasPropertoes =
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

        if (typeMatches && hasPropertoes)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory");
}

BufferWithMemory createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits,
            properties)};

    vk::raii::DeviceMemory bufferMemory(device, allocInfo);

    buffer.bindMemory(*bufferMemory, 0);

    return {
        std::move(buffer),
        std::move(bufferMemory)};
}

void createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferWithMemory bufferWithMemory = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        uniformBuffers.emplace_back(std::move(bufferWithMemory.buffer));
        uniformBuffersMemory.emplace_back(std::move(bufferWithMemory.memory));

        uniformBuffersMapped.push_back(
            uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding};

    descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void createDescriptorPool()
{
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize};

    descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT,
        *descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInf{
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data()};

    descriptorSets = vk::raii::DescriptorSets(device, allocInf);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *uniformBuffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject)};

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = *descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr};

        device.updateDescriptorSets(descriptorWrite, nullptr);
    }
}

int main()
{
    std::cout << "Spaceguy running\n";

    initWindow();
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();

    createDescriptorSetLayout();
    createGraphicsPipeline();

    createCommandPool();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createCommandBuffers();
    createSyncObjects();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawFrame();
    }

    device.waitIdle();

    cleanup();

    return 0;
}