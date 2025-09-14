#pragma once

#include "mari.hpp"

#include "buffer.hpp"
#include "components/camera.hpp"
#include "keyboard_controller.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "systems/ray_tracing_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <chrono>
#include <cassert>
#include <stdexcept>

namespace mari {
    Mari::Mari() {
        DefaultObjects::initialize(device);

        gui = std::make_unique<Gui>(device, window, renderer, rayTracingSystem);
        loadScene();

        for (auto const &[id, node] : scene->nodes) {
            node->start();
        }

        rayTracingSystem.buildScene(*scene);
        rayTracingSystem.tonemapper = 2;

        gui->set(scene);

        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT * 2)
            // Rasterization
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            // Ray Tracing
            .addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT * 3)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene->images.size() + scene->lightObjects.size()))

            .build();
    }

    Mari::~Mari() {
        DefaultObjects::cleanup(device);
    }

    void Mari::run() { 
        std::vector<std::unique_ptr<Buffer>> rasterizationUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> rayTracingUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> infiniteLightUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> lightUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
            rasterizationUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(RasterizationUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            rasterizationUboBuffers[i]->map();
            rayTracingUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(RayTracingUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            rayTracingUboBuffers[i]->map();
            infiniteLightUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(InfiniteLightUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            infiniteLightUboBuffers[i]->map();
            lightUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(LightUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            lightUboBuffers[i]->map();
        }

        DescriptorSetLayout rasterizationSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        /*
         * 0 -> Top-Level Acceleration Structure
         * 1 -> Accumulation image
         * 2 -> Present image
         * 3 -> Path tracing properties uniform
         * 4 -> Environment map data uniform
         * 5 -> Lights data uniform
         * 6 -> Mesh information buffer
         * 7 -> Textures buffer
         */
        const uint32_t imageCount = static_cast<uint32_t>(scene->images.size() + scene->lightObjects.size());
        DescriptorSetLayout rayTracingSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .addBinding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 
                        imageCount, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) // TODO if image count == 1, call VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER instead of variable descriptor
            .build();

        std::vector<VkDescriptorSet> rasterizationDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rasterizationDescriptorSets.size(); i++) {
            DescriptorWriter(rasterizationSetLayout, *globalPool)
                .writeBuffer(0, rasterizationUboBuffers[i]->descriptorInfo())
                .build(rasterizationDescriptorSets[i]);
        }

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout.handle()};
        PointLightSystem pointLightSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout.handle()};
        
        rayTracingSystem.buildPipeline(rayTracingSetLayout.handle());

        std::vector<VkDescriptorImageInfo> textureDescriptors{};
        for (auto &image : scene->images) {
            textureDescriptors.push_back(image->descriptorInfo());
        }
        for (auto &node : scene->lightObjects) {
            textureDescriptors.push_back(std::dynamic_pointer_cast<InfiniteAreaLight>(node->light)->equalAreaImage->descriptorInfo());
        }

        std::vector<VkDescriptorSet> rayTracingDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
            DescriptorWriter(rayTracingSetLayout, *globalPool)
                .writeAccelerationStructure(0, rayTracingSystem.tlas->descriptor())
                .writeImage(                1, rayTracingSystem.accumImage->descriptorInfo())
                .writeImage(                2, rayTracingSystem.presentImage->descriptorInfo())
                .writeBuffer(               3, rayTracingUboBuffers[i]->descriptorInfo())
                .writeBuffer(               4, infiniteLightUboBuffers[i]->descriptorInfo())
                .writeBuffer(               5, lightUboBuffers[i]->descriptorInfo())
                .writeBuffer(               6, rayTracingSystem.pPrimMeshesInfosBuffer->descriptorInfo())
                .writeImages(               7, &textureDescriptors)
                .build(rayTracingDescriptorSets[i]);
        }

        // TODO how to keep multiple pointers for each callback
        //glfwSetWindowUserPointer(window.getGLFWwindow(), &camera);
        //glfwSetScrollCallback(window.getGLFWwindow(), camera.scrollCallback);
        glfwSetWindowUserPointer(window.getGLFWwindow(), &window);

        std::shared_ptr<Node> cameraObject = std::make_shared<Node>();
        cameraObject->name = "Free Camera";
        cameraObject->camera = std::make_shared<Camera>();
        cameraObject->transform.position.y = 0.7f;
        cameraObject->transform.position.z = 1.5f;
        cameraObject->isStatic = false;
        scene->addNode(cameraObject);
        scene->cameraObjects.emplace_back(cameraObject);
        scene->currentCamera = cameraObject;
        
        KeyboardController controller{*gui};
        
        auto startTime      = std::chrono::high_resolution_clock::now();
        auto currentTime    = startTime;
        int  frameCounter   = 0;

        while (!window.shouldClose()) {
            glfwPollEvents();

            auto  newTime     = std::chrono::high_resolution_clock::now();
            float deltaTime   = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            float elapsedTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - startTime).count();
            currentTime = newTime;
            frameCounter++;

            // If window is resized and aspect ratio is different
            float aspect = renderer.getAspectRatio();
            scene->currentCamera->camera->setAspectRatio(aspect);

            // TODO on camera move when free camera isn't set, set free camera's transform equal to current camera and change to free camera
            controller.update(window.getGLFWwindow(), deltaTime, *scene->currentCamera);
            scene->update();

            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameCounter,
                    deltaTime,
                    elapsedTime,
                    commandBuffer,
                    *scene->currentCamera,
                    controller.isRayTracingOn() ? rayTracingDescriptorSets[frameIndex] : rasterizationDescriptorSets[frameIndex],
                    scene->nodes
                };

                if (controller.isRayTracingOn()) {
                    // update
                    RayTracingUbo ubo{};
                    ubo.viewInverse         = scene->currentCamera->camera->getInverseView();
                    ubo.projInverse         = scene->currentCamera->camera->getInverseProjection();
                    frameCounter            = controller.checkFrameAccumulationReset() ? 0 : frameCounter;
                    ubo.frameCount          = frameCounter;
                    ubo.maxDepth            = rayTracingSystem.maxDepth;
                    ubo.frameAccumulation   = static_cast<int>(rayTracingSystem.frameAccumulation);
                    ubo.russianRoulette     = static_cast<int>(rayTracingSystem.russianRoulette);
                    ubo.nextEventEstimation = static_cast<int>(rayTracingSystem.nextEventEstimation);
                    ubo.exposure            = rayTracingSystem.exposure;
                    ubo.tonemapper          = rayTracingSystem.tonemapper;
                    ubo.samplesPerPixel     = rayTracingSystem.samplesPerPixel;
                    rayTracingUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rayTracingUboBuffers[frameIndex]->flush();

                    const int lightID = scene->environmentID - static_cast<int>(scene->images.size());
                    const InfiniteAreaLight* currentInfiniteLight = std::dynamic_pointer_cast<InfiniteAreaLight>(scene->lightObjects[lightID]->light).get();
                    InfiniteLightUbo infiniteLight = { // TODO diferent descriptor set
                        scene->environmentID,
                        rayTracingSystem.environmentRotation,
                        currentInfiniteLight->sampler.getMarginal()->getIntegral(),
                        glm::uvec2(currentInfiniteLight->textureSize, currentInfiniteLight->textureSize),
                        currentInfiniteLight->sampler.getMarginal()->getFunctionBuffer()->deviceAddress(),
                        currentInfiniteLight->sampler.getMarginal()->getCdfBuffer()->deviceAddress(),
                        currentInfiniteLight->sampler.conditionalIntegralsBuffer->deviceAddress(),
                        currentInfiniteLight->sampler.conditionalFunctionsBuffer->deviceAddress(),
                        currentInfiniteLight->sampler.conditionalCdfsBuffer->deviceAddress(),
                    };

                    infiniteLightUboBuffers[frameIndex]->writeToBuffer(&infiniteLight);
                    infiniteLightUboBuffers[frameIndex]->flush();

                    LightUbo lightUbo = {
                        rayTracingSystem.lightsBuffer->deviceAddress(),
                        rayTracingSystem.lightsSampler.getFunctionBuffer()->deviceAddress(),
                        rayTracingSystem.lightsSampler.getCdfBuffer()->deviceAddress(),
                        rayTracingSystem.lightsSampler.getIntegral(),
                        static_cast<int>(rayTracingSystem.lights.size()),
                    };

                    lightUboBuffers[frameIndex]->writeToBuffer(&lightUbo);
                    lightUboBuffers[frameIndex]->flush();

                    // render
                    rayTracingSystem.render(frameInfo, renderer.getSwapchain());
                }
                /*
                else {
                    // update
                    RasterizationUbo ubo{};
                    ubo.projection  = currentCamera->camera->getProjection();
                    ubo.view        = currentCamera->camera->getView();
                    ubo.inverseView = currentCamera->camera->getInverseView();
                    pointLightSystem.update(frameInfo, ubo);
                    rasterizationUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rasterizationUboBuffers[frameIndex]->flush();
                    
                    // render
                    renderer.beginSwapchainRenderPass(commandBuffer);
                    simpleRenderSystem.render(frameInfo);
                    pointLightSystem.render(frameInfo);
                    renderer.endSwapchainRenderPass(commandBuffer);
                }
                */

                // GUI
                if (gui->isActive) {
                    gui->prepare(frameInfo);
                    renderer.beginSwapchainRenderPass(commandBuffer);
                    gui->render(commandBuffer);
                    renderer.endSwapchainRenderPass(commandBuffer);
                }

                renderer.endFrame();
/*
                if (frameCounter == 32) {
                    vkDeviceWaitIdle(device.handle());
                    gui->saveImage(*rayTracingSystem.presentImage);
                }
*/
                if (window.wasWindowResized()) { // TODO make this cleaner
                    window.resetWindowsResizedFlag();
                    frameCounter = 0; // TODO put everyone in one place;

                    vkDeviceWaitIdle(device.handle());

                    VkExtent2D e = window.getExtent();
                    rayTracingSystem.accumImage->resize(e.width, e.height);
                    rayTracingSystem.presentImage->resize(e.width, e.height);

                    for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
                        DescriptorWriter(rayTracingSetLayout, *globalPool)
                            .writeImage(1, rayTracingSystem.accumImage->descriptorInfo())
                            .writeImage(2, rayTracingSystem.presentImage->descriptorInfo())
                            .overwrite(rayTracingDescriptorSets[i]);
                    }
                }
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    void Mari::loadScene() {
        switch (  11  ) {
            case 0:
                scene = std::make_shared<Scene>(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.glb");
                scene->transform.position = {0.0f, -0.01f, 0.0f};
                break;
            case 1:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/ABeautifulGame/glTF/ABeautifulGame.glTF"); // TODO this scene uses instancing
                break;
            case 2:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/mylivingroom.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/BreakfastRoom/BreakfastRoom.gltf");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/city-many-lights-package/city-many-lights-v3-smaller.gltf");
                break;
            case 3:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sportsCar.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sibenik.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/SanMiguel.glb");
                break;
            case 4:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/OrientationTest/glTF-Binary/OrientationTest.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/GearboxAssy/glTF-Binary/GearboxAssy.glb");
                break;
            case 5:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/IntelSponza/intelsponza_curtains_ivy.glb");
                break;
            case 6:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/box2.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/xyzrgb_dragon_floor.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/DragonAttenuation.glb");
                break;
            case 7:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/bistro_exterior.glb");
                break;
            case 8:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/MetalRoughSpheres/glTF-Binary/MetalRoughSpheres.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/TextureLinearInterpolationTest/glTF-Binary/TextureLinearInterpolationTest.glb");
                break;
            case 9:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/deccer-cubes/SM_Deccer_Cubes_Textured_Complex.gltf");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sphere.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/cube.glb");
                break;
            case 10:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sketchfab/free_1975_porsche_911_930_turbo.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/Scenes/nvidia-attic.gltf");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/Scenes/mitsuba-knob.gltf");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/veach_mis_remake.glb");
                break;
            case 11:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/CornellBox/CornellBox-Boxes.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/CornellBox/cornell_dragons2.glb");
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/CornellBox/CornellBox-Spheres-Improved.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/CornellBox/Cornell-Volume.glb");
        }
        scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);

        std::vector<std::shared_ptr<InfiniteAreaLight>> lights{};
        lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, DefaultObjects::getImageWhite32f(), 1));
        lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, DefaultObjects::getImageBlack32f(), 1));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/zhengyang_gate_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "zhengyang_gate")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/brown_photostudio_01_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "brown_photostudio")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../../../_Models/gltf/IntelSponza/main1_sponza/textures/kloppenheim_05_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "kloppenheim_05_4k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/solitude_interior_8k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "solitude_interior_8k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/meadow_8k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "meadow_8k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/qwantani_noon_8k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_noon_8k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/qwantani_night_puresky_8k_darkened.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_night_puresky_8k_darkened")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/the_sky_is_on_fire_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "the_sky_is_on_fire_4k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/qwantani_late_afternoon_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_late_afternoon_puresky_4k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/sunny_vondelpark_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "sunny_vondelpark_4k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/kloofendal_48d_partly_cloudy_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "kloofendal_48d_partly_cloudy_puresky_4k")));
        //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, scene->loadImage("../../models/kloofendal_48d_partly_cloudy_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "kloofendal_48d_partly_cloudy_puresky_4k")));

        //gui->saveImageFromData((void *)lights[0]->imageBuffer->getMappedMemory(), 4096, 4096, 4, true);

        /*
        std::vector<float> data;
        for (const PiecewiseConstant1D& pie : lights[0]->sampler.pConditional) {
            for (auto& f : pie.getFunction()) {
                data.push_back(f);
            }
        }
        gui->saveImageFromData(data.data(), 4096, 4096, 1, true);
        */

        for (std::shared_ptr<InfiniteAreaLight> l : lights) {
            auto node = std::make_shared<Node>();
            node->name = l->equalAreaImage->name;
            node->light = l;
            scene->lightObjects.emplace_back(node);
        }

        scene->environmentID = static_cast<int>(scene->images.size() + scene->lightObjects.size() - 1);
        scene->start();
    }
}