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

So, it looks like we can have multiple descriptor set layouts. We reference them in the shader
by using the "set" before the "binding". This can be cool to have things like one for
global stuff, like the camera, one for materials, etc...

A descriptor set layout is the contract between Vulkan and the shaders.

A descriptorPool is the allocator for descriptor sets, basically:

"I need space for N uniform buffer descriptors"
"I need space for N image sampler descriptors"
"I want to allocate up to N descriptor sets"

The descriptor set is the actual resource.

We use one descriptor set per frame in flight or one per frame basically because each frame should have it's own uniforms and data (I need to check when it is with per vertex data).