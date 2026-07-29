# TODO

- [ ] Use a memory allocator: https://gpuopen.com/vulkan-memory-allocator/
- [ ] Figure out how to handle multiple glfw window callbacks
- [ ] Quaternions
- [ ] Create a debug and release path

## Ray tracing pipeline

- [ ] Store tonemapped result directly in swapchain image if possible
- [ ] BLAS compacting
- [ ] Better bounding boxes / bvh
- [ ] Instancing
- [x] Set opaque meshes
- [x] Update acceleration structure on transform changes
- [x] Rebuild acceleration structure on material changes
- [ ] Update/rebuild light importance sampling on envmap or emissive changes
- [x] Move all logic to ray generation shaders

## Path Tracing

- [x] Materials
    - [x] Lambert
    - [x] Metallic
    - [x] Dielectric
- [ ] Implement multiple scattering for energy conservation in high roughness materials
- [x] Tonemapping
- [x] Normal mapping
- [ ] Ray differentials for texturing
- [x] Scale transmission by etaScale for stable russian roulette according to pbrt v4
- [ ] Bidirectional path tracing
- [ ] Spectral rendering
- [ ] Implement better noise than just white noise

## Participating Media

- [x] NanoVDB
- [x] Mesh bounded volumes
- [x] Importance sampling
- [ ] Transmittance estimators:
  - [ ] Ray marching
  - [ ] Unbiased Ray marching
  - [x] Delta tracking
  - [ ] Ratio tracking
- [ ] Phase functions:
    - [x] Isotropic
    - [ ] Rayleigh 
    - [x] Henyey-Greenstein
    - [x] Approximate Mie Scattering
- [ ] Spectrally variant phase functions

## glTF

- [ ] No longer force 4 color channels for loaded textures and add more format versatility
- [ ] Import image buffers directly without stb_image
- [ ] See if min and max properties from glTF position accessors can be used for better bounding boxes (although they might not be aligned after transformations)

## imGui

- [ ] Create a custom theme

# Research

- [ ] Ray tracing Shader Execution Reordering (SER) VK_RAY_TRACING_INVOCATION_REORDER_MODE_REORDER_NV / GL_NV_shader_invocation_reorder
