#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Everything related with Vulkan states.
struct VulkanContext
{
    GLFWwindow *window = nullptr;
    vk::raii::Context context;
    vk::raii::Instance instance{nullptr};
    vk::raii::SurfaceKHR surface{nullptr};
    vk::raii::PhysicalDevice physicalDevice{nullptr};
    vk::raii::Device device{nullptr};
    vk::raii::Queue graphicsQueue{nullptr};
    vk::raii::Queue presentQueue{nullptr};
    vk::raii::CommandPool commandPool{nullptr};
    vk::raii::SwapchainKHR swapchain{nullptr};
    std::vector<vk::Image> swapchainImages;
    std::vector<vk::raii::ImageView> swapchainImageViews;
    vk::Format swapchainImageFormat;
    vk::Extent2D swapchainExtent;
};

extern VulkanContext vulkanContext;

// Step 1: having Vulkan ready to be used.
// It also creates and configure the swapchain... We may make it more modular.
void setupVulkan();
