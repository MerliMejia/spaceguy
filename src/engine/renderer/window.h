#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#error Unsupported platform
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <functional>
#include <iostream> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

namespace Renderer {

struct Window {
  GLFWwindow *handler = nullptr;
  vk::raii::SurfaceKHR surface = nullptr;

  void init(const uint32_t width, const uint32_t heigth, std::string name) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    handler = glfwCreateWindow(width, heigth, name.c_str(), nullptr, nullptr);
  }

  void update(std::function<void()> callback) {
    while (!glfwWindowShouldClose(handler)) {
      glfwPollEvents();
      callback();
    }
  }

  void createSurface(vk::raii::Instance &instance) {
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, handler, nullptr, &_surface) != 0) {
      throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
  }

  void cleanup() {
    glfwDestroyWindow(handler);
    glfwTerminate();
  }
};

} // namespace Renderer
