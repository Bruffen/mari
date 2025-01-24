# TODO

- Create separate buffers for mesh data and link device addresses to shaders
- Methods in device class with buffer/image logic should go to their respective buffer and image classes

- Create mesh class with default cube, sphere, etc mesh builders
- Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- Double check which methods should be const
- Tonemapping compute shader
- Figure out how to handle multiple glfw window callbacks

## glTF

- No longer force 4 color channels for loaded textures and add more format versatility
- Import image buffers directly without stb_image
- Import camera and add switching between existing cameras

## imGui

- Docking not working
- Imgui input and camera drag can't happen simultaneously

# Research
- Ray tracing Shader Execution Reordering (SER) VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV / GL_NV_shader_invocation_reorder
