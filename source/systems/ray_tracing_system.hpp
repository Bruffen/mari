#pragma once

#include "acceleration_structure.hpp"
#include "pipeline.hpp"
#include "pipeline_layout.hpp"
#include "scene/scene.hpp"
#include "frame_info.hpp"
#include "image.hpp"

namespace mari {
    enum Integrator {
        PATH_TRACING,
        PATH_TRACING_VOLUME_ONLY
    };

    class RayTracingSystem {
        public:
            RayTracingSystem(Device &device, Window &window);
            ~RayTracingSystem();

            void                                    render(FrameInfo &frameInfo, Swapchain &swapchain);
            void                                    buildScene(const Scene &scene);
            void                                    buildPipeline(VkDescriptorSetLayout descriptorSetLayout, Integrator integrator);
            void                                    createAreaLights(const Scene &scene);
            
            // TODO get around having these be public
            std::unique_ptr<AccelerationStructure>  tlas;
            std::unique_ptr<Image>                  accumImage;
            std::unique_ptr<Image>                  presentImage;
            std::unique_ptr<Buffer>                 primMeshesInfosBuffer;
            std::unique_ptr<Buffer>                 pPrimMeshesInfosBuffer;
            std::vector<LightInfo>                  lights;
            std::unique_ptr<Buffer>                 lightsBuffer;
            PiecewiseConstant1D                     lightsSampler;
            glm::vec2                               environmentRotation{};

            
            int                                     maxDepth            = 20;
            bool                                    frameAccumulation   = true;
            bool                                    russianRoulette     = true;
            bool                                    nextEventEstimation = true;
            float                                   exposure            = 1.0f;
            int                                     tonemapper          = 2;
            int                                     samplesPerPixel     = 1;
        private:
            void                                    buildBLAS(const Mesh &mesh, VkTransformMatrixKHR transformMatrix, uint64_t materialBufferDeviceAddress);
            void                                    buildTLAS();
            void                                    createImages(uint32_t width, uint32_t height);

            Device                                  &device;
            Integrator                              integrator{};
            std::vector<std::unique_ptr<AccelerationStructure>> blases{};
            std::unique_ptr<Pipeline>               pipeline{}; 
            std::unique_ptr<PipelineLayout>         pipelineLayout{};
            std::vector<PrimMeshInfo>               primMeshesInfos{};

    };
}