# Agent Instructions

AI agents working in this folder must not change, edit, add, delete, move, rename, or generate any files here.

When asked to make a change in this folder, agents should only explain how the requester can do it themselves. Provide clear guidance, commands, or code snippets as needed, but do not perform the action directly.

# Vulkan Direction

This project should follow the current Khronos Vulkan Tutorial approach:

- Use Vulkan-Hpp RAII (`vk::raii`) instead of raw Vulkan C handles where practical.
- Use dynamic rendering instead of legacy `VkRenderPass` and `VkFramebuffer` render paths.
- Prefer the modern tutorial baseline: Vulkan 1.4 when available, with Vulkan 1.3 compatibility when required by the local SDK or platform.
- Do not add new render-pass/framebuffer based tutorial code unless the requester explicitly asks for the legacy path.
- When explaining implementation steps, reference the latest Khronos Vulkan Tutorial at `https://docs.vulkan.org/tutorial/latest/`.

# Vulkan-Hpp Constructor Style

This project uses:

```cpp
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

Prefer aggregate/designated initialization compatible with VULKAN_HPP_NO_CONSTRUCTORS, for example:

vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
    .vertexBindingDescriptionCount = bindingCount,
    .pVertexBindingDescriptions = bindings.data(),
    .vertexAttributeDescriptionCount = attributeCount,
    .pVertexAttributeDescriptions = attributes.data(),
};
```
