# TODO

- Create separate buffers for mesh data and link device addresses to shaders
- Methods in device class with buffer/image logic should go to their respective buffer and image classes

- Create mesh class with default cube, sphere, etc mesh builders
- Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- ImGUI (start with showing framerate / frame duration)
- tinygltf / fastgltf
- Double check which methods should be const
- Tonemapping compute shader
- Figure out how to handle multiple glfw window callbacks

# Research
- Ray tracing Shader Execution Reordering (SER) VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV / GL_NV_shader_invocation_reorder
