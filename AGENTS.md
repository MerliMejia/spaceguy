# Agent Instructions

AI agents working in this folder must not change, edit, add, delete, move, rename, or generate any files here.

When asked to make a change in this folder, agents should only explain how the requester can do it themselves. Provide clear guidance, commands, or code snippets as needed, but do not perform the action directly.

By default, do not provide drop-in/copy-paste ready code, explain wha the user needs to do, answer he's questions and only if the user is explicitly
telling you to provide the drop-in/copy-paste ready code you provide it.

# Code Explanation Style

When I ask how to do something with code, explain it as a general, self-contained example. Do not inspect or adapt the example to the project’s current implementation, and do not optimize it for direct copy-and-paste use.

Prioritize teaching the concepts, responsibilities, and sequence of operations so I can understand the mechanism independently of the project architecture.

Only inspect and tailor the explanation to the existing project when I explicitly ask how to implement it in a particular file, component, system, or part of the project.

# Shader Language and Extension Policy

- Use Slang for all shader code and shader examples.
- Do not provide GLSL, HLSL, or other shader-language implementations unless explicitly requested.
- Compile Slang shaders to SPIR-V for Vulkan.
- Use Slang-native syntax and facilities, such as `NonUniformResourceIndex`, instead of GLSL-specific constructs such as `nonuniformEXT`.
- Do not use, enable, recommend, or depend on Vulkan extensions or shader-language extension directives.
- Prefer functionality available in the targeted core Vulkan version.
- Core Vulkan features must still be queried and enabled when required; being part of core Vulkan does not imply that every feature is automatically active.
- If a requested feature cannot be implemented without an extension, explain the limitation and ask before presenting an extension-based approach.

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
