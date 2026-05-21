Mental notes to remember:

When it comes to a Vulkan renderer:

1- Things that you create once and reuse:

- Instance
- Surface
- Physical device
- Logical device
- Graphics and Present queue
- Swapchain, Swapchain images, Swapchain image formats, Swapchain extent (All of these needs to be recreated when the swapchain is recreated)
- Command pool
- Command buffers
- Semaphores for available images, for render finished flag.
- Fences for frame in flights.

- - All of the above is for "Creating a Vulkan app that renders things".

2 - Things that are reusable, but we need to be able to shape them at least once.

- Graphics pipeline (Depending on what you're trying to render, the shaders used, shader inputs, data sent to the shader). Is kind of "the way of drawing something".
- - All of these are tied to the graphics pipeline and we may want to create them when defining a graphics pipeline:
- - - Pipeline layout (Used by the graphics pipeline)
- - - Descriptor set layout (used by the Pipeline layout)
- - - Descriptor set (Described in shape by the descriptor ser layout)
- - - Uniform buffer (The descriptor set points to it).

3 - After that it all depneds on the scene itself, so it is creating and setting up things to draw, syncronization and the actual draw commands