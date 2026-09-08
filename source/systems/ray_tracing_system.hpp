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
        PATH_TRACING_VOLUME_ONLY,
        PATH_TRACING_VOLUMETRIC_AHIT,   // Shadow ray returns a list of any hits with media which are sorted after
        PATH_TRACING_VOLUMETRIC_CHIT,   // Shadow ray is a loop of closest hits through media until light is reached
    };

    class RayTracingSystem {
        public:
            RayTracingSystem(Device &device, Window &window);
            ~RayTracingSystem();

            void                                    buildScene(const Scene &scene);
            void                                    buildPipeline(VkDescriptorSetLayout descriptorSetLayout, Integrator integrator);
            void                                    update(const Scene &scene);
            void                                    render(FrameInfo &frameInfo, Swapchain &swapchain);

            Integrator                              getIntegrator() { return integrator; }
            
            // TODO get around having these be public
            std::unique_ptr<AccelerationStructure>  tlas;
            std::unique_ptr<Buffer>                 accelerationStructuresInstancesBuffer;
            std::unique_ptr<Image>                  accumImage;
            std::unique_ptr<Image>                  presentImage;
            std::unique_ptr<Buffer>                 primMeshesInfosBuffer;
            std::unique_ptr<Buffer>                 pPrimMeshesInfosBuffer;
            std::vector<LightInfo>                  lights;
            std::unique_ptr<Buffer>                 lightsBuffer;
            PiecewiseConstant1D                     lightsSampler;
            glm::vec2                               environmentRotation{};
            bool                                    needsUpdate         = false;
            bool                                    needsRebuild        = false;
            bool                                    needsLightsRebuild  = false;
            
            int                                     maxDepth            = 100;
            bool                                    frameAccumulation   = true;
            bool                                    russianRoulette     = true;
            bool                                    nextEventEstimation = true;
            float                                   exposure            = 1.0f;
            int                                     tonemapper          = 3;
            int                                     transmittanceAlgo   = 0;
            int                                     samplesPerPixel     = 1;
        private:
            void                                    createImages(uint32_t width, uint32_t height);
            void                                    buildAreaLights(const Scene &scene);
            void                                    buildBLAS(std::shared_ptr<Node> node, uint64_t materialBufferDeviceAddress);
            void                                    buildTLAS();
            void                                    updateTLAS();

            Device                                  &device;
            Integrator                              integrator{};
            std::vector<std::unique_ptr<
                AccelerationStructure>>             blases{};
            std::unique_ptr<Pipeline>               pipeline{}; 
            std::unique_ptr<PipelineLayout>         pipelineLayout{};
            std::vector<PrimMeshInfo>               primMeshesInfos{};

    };
}