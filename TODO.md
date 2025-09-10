# TODO

- Create mesh class with default cube, sphere, etc mesh builders
- Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- Double check which methods should be const
- Figure out how to handle multiple glfw window callbacks
- Quaternions
- Fix MikkTSpace
- Create a debug and release path

## Ray tracing pipeline

- Store tonemapped result directly in swapchain image if possible
- BLAS compacting
- Better bounding boxes / bvh
- Instancing
- Set opaque meshes
- Update/reconstruct acceleration structure on transform changes
- Update/reconstruct light importance sampling on envmap or emissive changes

## Path tTracing

- Normal mapping
- Ray differentials for texturing
- Scale transmission by etaScale for stable russian roulette according to pbrt v4

## glTF

- No longer force 4 color channels for loaded textures and add more format versatility
- Import image buffers directly without stb_image
- See if min and max properties from position accessors can be used for better bounding boxes (although they might not be aligned after transformations)

## imGui

- Change materials on the fly by writing over the material index in buffer (maybe show dropdown with material names instead of indices)
- Create a custom theme

# Research

- Ray tracing Shader Execution Reordering (SER) VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV / GL_NV_shader_invocation_reorder
