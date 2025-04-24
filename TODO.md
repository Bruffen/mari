# TODO

- Create separate buffers for mesh data and link device addresses to shaders

- Create mesh class with default cube, sphere, etc mesh builders
- Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- Double check which methods should be const
- Tonemapping compute shader
- Figure out how to handle multiple glfw window callbacks
- Quaternions
- Save image to file

## Ray tracing pipeline

- Store tonemapped result directly in swapchain image if possible
- BLAS compacting
- Better bounding boxes / bvh
- Instancing
- Set opaque meshes

## glTF

- No longer force 4 color channels for loaded textures and add more format versatility
- Import image buffers directly without stb_image
- See if min and max properties from position accessors can be used for better bounding boxes (although they might not be aligned after transformations)

## imGui


# Research

- Ray tracing Shader Execution Reordering (SER) VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV / GL_NV_shader_invocation_reorder
