#pragma once

#include "window.hpp"
#include "device.hpp"
#include "scene/scene.hpp"
#include "components/volume.hpp"

namespace mari {
    class TestScenes {
        public:
        static std::shared_ptr<Scene> loadScene(Device &device, Window &window, int sceneId) {
            std::shared_ptr<Scene> scene = std::make_shared<Scene>(device);

            std::vector<std::shared_ptr<InfiniteAreaLight>> lights{};
            lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, DefaultObjects::getImageWhite32f(), 1));
            lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, DefaultObjects::getImageBlack32f(), 1));
            lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/rustig_koppie_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "rustig_koppie_puresky_4k")));

            switch (  sceneId  ) {
                case 0:
                    scene->load("../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.glb");
                    scene->transform.position = {0.0f, -0.01f, 0.0f};
                    break;
                case 1:
                    scene->load("../../../../_Models/gltf/CornellBox/Cornell-Volume.glb");
                    scene->currentCamera = scene->nodes.at("Camera");
                    std::ranges::find(scene->materials, "light", [](const auto& m) { return m->name; })->get()->insideMedia = true;
                    break;
                case 2:
                    scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/ABeautifulGame/glTF/ABeautifulGame.glTF"); // TODO this scene uses instancing
                    break;
                case 3:
                    scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
                    //scene->load("../../../../_Models/gltf/sportsCar.glb");
                    //scene->load("../../../../_Models/gltf/sibenik.glb");
                    //scene->load("../../../../_Models/gltf/SanMiguel.glb");
                    break;
                case 4:
                    scene->load("../../../../_Models/gltf/IntelSponza/intelsponza_curtains_ivy.glb");
                    lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../../../_Models/gltf/IntelSponza/main1_sponza/textures/kloppenheim_05_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "kloppenheim_05_4k")));
                    break;
                case 5:
                    scene->load("../../../../_Models/gltf/DragonAttenuation.glb");
                    break;
                case 6:
                    //scene->load("../../../../_Models/gltf/xyzrgb_dragon_floor.glb");
                    scene->load("../../../../_Models/gltf/statues/hosmer-zenobia_in_chains.glb");
                    //scene->load("../../../../_Models/gltf/statues/lucy-stanford-repository.glb");
                    break;
                case 7:
                    //scene->load("../../../../_Models/gltf/bistro_exterior.glb");
                    //scene->load("../../../../_Models/gltf/Scenes/nvidia-attic.gltf");
                    scene->load("../../../../_Models/gltf/Scenes/scandinavian-studio-main-room.gltf");
                    //scene->load("../../../../_Models/gltf/UE4_SunTemple.glb");
                    break;
                case 8:
                    //scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/MetalRoughSpheres/glTF-Binary/MetalRoughSpheres.glb");
                    //scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/TextureLinearInterpolationTest/glTF-Binary/TextureLinearInterpolationTest.glb");
                    scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/AttenuationTest/glTF-Binary/AttenuationTest.glb");
                    //scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/NormalTangentTest/glTF-Binary/NormalTangentTest.glb");
                    //scene->load("../../../../_Models/gltf/glTF-Sample-Models/2.0/MosquitoInAmber/glTF-Binary/MosquitoInAmber.glb");
                    break;
                case 9:
                    scene->load("../../../../_Models/gltf/plane.glb");
                    scene->nodes.at("plane")->transform.scale *= 4.0f;
                    {
                        std::shared_ptr<Node> cam = std::make_shared<Node>();
                        cam->name = "Camera";
                        cam->camera = std::make_shared<Camera>();
                        cam->transform.position.x = 0.35f;
                        cam->transform.position.y = 0.7f;
                        cam->transform.position.z = 1.5f;
                        cam->isStatic = false;
                        scene->addNode(cam);
                        scene->cameraObjects.emplace_back(cam);
                        scene->currentCamera = scene->nodes.at("Camera");
                    }
                    window.resizeWindow(1024, 1024);
                    break;
                case 10:
                    //scene->load("../../../../_Models/gltf/sketchfab/free_1975_porsche_911_930_turbo.glb");
                    //scene->load("../../../../_Models/gltf/Scenes/mitsuba-knob.gltf");
                    scene->load("../../../../_Models/gltf/_myScenes/veach_mis_remake.glb");
                    //scene->load("../../../../_Models/gltf/_myScenes/ganesha_comparison.glb");
                    break;
                case 11:
                    //scene->load("../../../../_Models/gltf/CornellBox/CornellBox-Boxes.glb");
                    scene->load("../../../../_Models/gltf/CornellBox/CornellBox-Spheres-Improved.glb");
                    window.resizeWindow(1024, 1024);
                    break;
                case 12:
                    scene->load("../../../../_Models/gltf/Bruff_Lightning.glb");
                    break;
                case 13:
                    // TODO oriental lantern scene

                    break;
                case 14:
                    scene->load("../../../../_Models/gltf/_myScenes/ocean.glb");
                    break;
                case 15:
                    scene->load("../../../../_Models/gltf/_myScenes/opal.glb");
                    break;
                case 16:
                    scene->load("../../../../_Models/gltf/pica_pica_mini_diorama_01.glb");
                    break;
                case 17:
                    scene->load("../../../../_Models/gltf/Scenes/stilllife/stilllife.glb");
                    lights.emplace_back(
                        std::make_shared<InfiniteAreaLight>(
                            device, Scene::loadImage(device, "../../../../_Models/gltf/Scenes/stilllife/room.hdr", 
                            VK_FORMAT_R32G32B32A32_SFLOAT, "room")
                    ));
                break;
            }
            scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);

            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/citrus_orchard_road_puresky_1k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "citrus_orchard_road_puresky_1k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/solitude_interior_8k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "solitude_interior_8k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_late_afternoon_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_late_afternoon_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_moonrise_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_moonrise_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_noon_8k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_noon_8k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_noon_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_noon_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_sunset_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_sunset_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/qwantani_dusk_2_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "qwantani_dusk_2_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/dikhololo_night_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "dikhololo_night_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/the_sky_is_on_fire_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "the_sky_is_on_fire_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/sunny_vondelpark_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "sunny_vondelpark_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/kloofendal_48d_partly_cloudy_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "kloofendal_48d_partly_cloudy_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/rosendal_park_sunset_puresky_4k.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "rosendal_park_sunset_puresky_4k")));
            //lights.emplace_back(std::make_shared<InfiniteAreaLight>(device, Scene::loadImage(device, "../../models/sun_only.hdr", VK_FORMAT_R32G32B32A32_SFLOAT, "sun_only")));

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

    /*        scene->volumeObject = std::make_shared<Node>("volume");
    /*
            //scene->volumeObject->volume = std::make_unique<Volume>(device, "../../../../_Models/volumes/wdas_cloud/wdas_cloud_eighth.vdb");
            //scene->volumeObject->volume = std::make_unique<Volume>(device, "../../../../_Models/volumes/clouds_hr/cloud_cumulus_4_size_2.vdb");
            //scene->volumeObject->volume = std::make_unique<Volume>(device, "../../../../_Models/volumes/JangaFX - CloudPackVDB/CloudPack/CloudPackVDB/cloud_01_variant_0000.vdb");
            scene->volumeObject->transform.scale *= 0.005f;
            scene->volumeObject->volume->medium.scattering *= 80.0f;
            scene->volumeObject->volume->jitteringAmount = 0.0f;
            scene->volumeObject->volume->medium.phaseFunction.type = PhaseFunctionType::MieApproximation;
            scene->volumeObject->volume->medium.phaseFunction.particleSize = 20.0f;
    *//*
            scene->volumeObject->volume = std::make_unique<Volume>(device, "../../../../_Models/volumes/fire.vdb"); 
            //scene->volumeObject->volume = std::make_unique<Volume>(device, "../../../../_Models/volumes/explosion.vdb");
            scene->volumeObject->transform.scale *= 0.005f;
            scene->volumeObject->volume->medium.absorption = 1.0f;
            scene->volumeObject->volume->medium.scattering = 1.0f;

            scene->addNode(scene->volumeObject);
    */
            scene->environmentID = static_cast<int>(scene->images.size() + scene->lightObjects.size() - 1);
            return scene;
        }
    };
}