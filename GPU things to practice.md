GPU things to practice

[] Vertex Buffers (positions, normals, UVs, vertex colors, tangents?, per instance attribute)

[] Index Buffers (Reuse vertexs)

[] Uniforms (camera, frame level constants, material constants if not too many)

[] Storage buffers (Larger read/write or read-only buffers, many object transforms, large arrays)

[] Push constants (tiny pieces of data in the command buffer).

[] Sampled images / textures (color, normal, roughness, metalness, shadow maps, env maps)

[] Storage images (compute shader output, post processing)

// Possible things to practice:

Multiple queue submissions per frame: you may need extra semaphores between submits.
Separate graphics / compute / transfer queues: you may use semaphores or timeline semaphores for queue-to-queue ordering.
Streaming uploads: often use timeline semaphores or per-upload fences.
Per-swapchain-image resources: attachments, image views, or descriptor bindings tied to the acquired image may be indexed by imageIndex.
Per-frame transient resources: command buffers, staging arenas, uniform buffers, descriptor pools, and fences are commonly indexed by frameIndex.
Timeline semaphore architecture: can reduce the number of binary semaphores, especially in larger engines.
