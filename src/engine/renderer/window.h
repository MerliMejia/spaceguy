#pragma once
#include <GLFW/glfw3.h>

#include <functional>
#include <iostream> // IWYU pragma: keep
#include <string.h> // IWYU pragma: keep

namespace Renderer {

struct Window {
  GLFWwindow *handler = nullptr;

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

  void cleanup() {
    glfwDestroyWindow(handler);
    glfwTerminate();
  }
};

} // namespace Renderer
