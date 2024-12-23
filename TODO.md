# TODO

- Create separate buffers for mesh data and link device addresses to shaders
- ~~Swapchain recreation when window resizing needs command buffers to be recorded every frame.~~ RT storage image needs to be recreated along with the window resizing
- Methods in device class with buffer/image logic should go to their respective buffer and image classes

- Create mesh class with default cube, sphere, etc mesh builders
- Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- ImGUI (start with showing framerate / frame duration)
- tinygltf / fastgltf
- Double check which methods should be const
- Tonemapping compute shader

# Research
- Figure out how to handle multiple glfw window callbacks

