#pragma once

#include "scene.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/util.hpp>
#include <iostream>
#include <omp.h>

namespace mari {
    constexpr std::size_t recoverLastSeparatorPos(std::size_t firstSeparatorPos, std::size_t secondSeparatorPos, std::size_t invalidPos) {
        if (firstSeparatorPos == invalidPos)
            return secondSeparatorPos;

        // The first separator has been found
        if (secondSeparatorPos == invalidPos)
            return firstSeparatorPos;

        // Both separators have been found; the max (rightmost) position must be picked
        return std::max(firstSeparatorPos, secondSeparatorPos);
    }

    inline std::size_t recoverLastSeparatorPos(const std::string& pathStr) {
        const std::size_t lastSlashPos     = pathStr.find_last_of('/');
        const std::size_t lastBackslashPos = pathStr.find_last_of('\\');

        return recoverLastSeparatorPos(lastSlashPos, lastBackslashPos, std::string::npos);
    }

    std::string recoverPathToFile(const std::string& pathStr) {
        return pathStr.substr(0, recoverLastSeparatorPos(pathStr) + 1);
    }

    Scene::Scene(Device &device, const std::string &path) : device{device} {
        constexpr auto gltfOptions = 
            fastgltf::Options::DontRequireValidAssetMember | 
            fastgltf::Options::AllowDouble | 
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::DecomposeNodeMatrices;

        fastgltf::Parser parser {};
        fastgltf::Expected<fastgltf::GltfDataBuffer> data = fastgltf::GltfDataBuffer::FromPath(path);
        if (data.error() != fastgltf::Error::None) {
            throw std::invalid_argument("Error: Could not load the glTF file.");
        }
        const std::string folder = recoverPathToFile(path);
        fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(data.get(), folder, gltfOptions);
        if (asset.error() != fastgltf::Error::None) {
            throw std::invalid_argument("Error: Could not parse glTF file.");
        }

        fastgltf::Asset& gltf = asset.get();

        // TODO increase descriptor pool size

        // Temporary arrays for all the objects to use while creating the GLTF data
        std::vector<std::shared_ptr<GameObject>> tmpNodes;
        std::vector<uint32_t>                    tmpImageIds;
        std::vector<std::shared_ptr<Camera>>     tmpCameras;

        loadSamplers(gltf.samplers);
        loadImages(gltf.images, gltf, folder); // TODO improve passing asset gltf
        loadMaterials(gltf.materials, gltf.textures);
        loadMeshes(gltf);

        // load cameras
        for (auto& camera : gltf.cameras) {
            std::shared_ptr<Camera> newCamera = std::make_unique<Camera>();
            tmpCameras.emplace_back(newCamera);
            std::visit(fastgltf::visitor {
                [&](fastgltf::Camera::Perspective& perspective) {
                    newCamera->setPerspectiveProjection(
                        perspective.yfov, 
                        perspective.aspectRatio.value_or(16.0f/9.0f), 
                        perspective.znear, 
                        perspective.zfar.value_or(1000.0f)
                    );
                },
                [&](fastgltf::Camera::Orthographic& orthographic) {
                    newCamera->setOrthographicProjection(
                        -orthographic.xmag * 0.5f, 
                         orthographic.xmag * 0.5f, 
                         orthographic.ymag * 0.5f, 
                        -orthographic.ymag * 0.5f,
                        orthographic.znear,
                        orthographic.zfar
                    );
                },
            }, camera.camera);
        }

        // load all nodes and their meshes
        for (fastgltf::Node& node : gltf.nodes) {
            std::shared_ptr<GameObject> newNode = std::make_shared<GameObject>();
            newNode->name = node.name.c_str();

            if (node.meshIndex.has_value()) {
                newNode->mesh = meshes[*node.meshIndex];
            }
            if (node.cameraIndex.has_value()) {
                newNode->camera = tmpCameras[*node.cameraIndex];
                cameras.emplace_back(newNode);
            }

            tmpNodes.emplace_back(newNode);
            nodes[newNode->name] = newNode;

            std::visit(fastgltf::visitor { [&](fastgltf::math::fmat4x4 matrix) {
                                            //memcpy(&newNode->, matrix.data(), sizeof(matrix));
                                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                    glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                    glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                    newNode->transform.position = glm::vec3(transform.translation.x(), transform.translation.y(), transform.translation.z());
                    newNode->transform.rotation = glm::eulerAngles(rot);
                    newNode->transform.scale    = glm::vec3(transform.scale.x(), transform.scale.y(), transform.scale.z());
                } },
                node.transform
            );
        }

        // run loop again to setup transform hierarchy
        for (int i = 0; i < gltf.nodes.size(); i++) {
            fastgltf::Node& node = gltf.nodes[i];
            std::shared_ptr<GameObject>& sceneNode = tmpNodes[i];

            for (auto& c : node.children) {
                sceneNode->children.emplace_back(tmpNodes[c]);
                tmpNodes[c]->parent = sceneNode;
            }
        }

        // find the top nodes, with no parents
        for (auto& node : nodes) {
            if (node.second->parent.lock() == nullptr) {
                topNodes.emplace_back(node.second);
            }
        }
    }

    void Scene::update() {
        for (auto& g : topNodes) {
            updateWorldMatrix(*g, transform.mat4());
        }
    }

    void Scene::updateWorldMatrix(GameObject& g, const glm::mat4 &worldMatrix) { // TODO don't update for static objects after the first time (maybe have a second set function)
        g.worldMatrix = worldMatrix * g.transform.mat4();
        for (auto& c : g.children) {
            updateWorldMatrix(*c, g.worldMatrix);
        }
    }

    void Scene::loadMeshes(const fastgltf::Asset &gltf) {

        std::vector<uint32_t> indices;
        std::vector<Mesh::Vertex> vertices;

        for (const fastgltf::Mesh& mesh : gltf.meshes) {
            std::shared_ptr<Mesh> newMesh = std::make_shared<Mesh>(device);
            newMesh->name = mesh.name;

            // clear the mesh arrays each mesh, we dont want to merge them by error
            indices.clear();
            vertices.clear();

            for (const fastgltf::Primitive &p : mesh.primitives) {
                SubMesh newPrimitive;
                newPrimitive.start = static_cast<uint32_t>(indices.size());
                newPrimitive.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

                uint32_t initial_vtx = static_cast<uint32_t>(vertices.size());

                // load indexes
                {
                    const fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);
                    
                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
                }

                // load vertex positions
                {
                    const fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        gltf, 
                        posAccessor,
                        [&](fastgltf::math::fvec3 v, size_t index) {
                            Mesh::Vertex newvtx;
                            newvtx.position = {v.x(), v.y(), v.z()};
                            newvtx.normal = { 1, 0, 0 };
                            newvtx.color = glm::vec4 { 1.f };
                            newvtx.uv = {0, 0};
                            vertices[initial_vtx + index] = newvtx;
                        }
                    );
                }

                // load vertex normals
                auto normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                        [&](fastgltf::math::fvec3 v, size_t index) {
                            vertices[initial_vtx + index].normal = {v.x(), v.y(), v.z()};
                        });
                }

                // load UVs
                auto uv = p.findAttribute("TEXCOORD_0");
                if (uv != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                        [&](fastgltf::math::fvec2 v, size_t index) {
                            vertices[initial_vtx + index].uv = {v.x(), v.y()};
                        });
                }

                // load vertex colors
                auto colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                        [&](fastgltf::math::fvec4 v, size_t index) {
                            vertices[initial_vtx + index].color =  {v.x(), v.y(), v.z(), v.w()};
                        });
                }

                if (p.materialIndex.has_value()) {
                    newPrimitive.material = materials[p.materialIndex.value()];
                } else {
                    newPrimitive.material = materials[0];
                }

                newMesh->submeshes.push_back(newPrimitive);
            }

            newMesh->createVertexBuffers(vertices);
            newMesh->createIndexBuffers(indices);
            meshes.emplace_back(newMesh);
        }
    }


    void Scene::loadMaterials(const std::vector<fastgltf::Material> &gltfMaterials, const std::vector<fastgltf::Texture> &gltfTextures) {
        for (const fastgltf::Material& gltfMat : gltfMaterials) {
            std::shared_ptr<Material> m = std::make_shared<Material>();
            m->name                 = gltfMat.name.c_str();
            m->constants.albedo.x   = gltfMat.pbrData.baseColorFactor[0];
            m->constants.albedo.y   = gltfMat.pbrData.baseColorFactor[1];
            m->constants.albedo.z   = gltfMat.pbrData.baseColorFactor[2];
            m->constants.albedo.w   = gltfMat.pbrData.baseColorFactor[3];
            m->constants.metallic   = gltfMat.pbrData.metallicFactor;
            m->constants.roughness  = gltfMat.pbrData.roughnessFactor;
            m->constants.emission   = glm::vec4(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2], gltfMat.emissiveStrength);

            /*
            if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend) {
                // TODO material transparency for pipeline pass
            }
            */

            // default the material textures
            m->resources.albedoImage                     = DefaultObjects::getImageWhite();
            m->resources.albedoImage->sampler            = DefaultObjects::getSamplerLinear();
            m->resources.metallicRoughnessImage          = DefaultObjects::getImageWhite();
            m->resources.metallicRoughnessImage->sampler = DefaultObjects::getSamplerLinear();

            // grab textures from gltf file
            if (gltfMat.pbrData.baseColorTexture.has_value()) {
                size_t img      = gltfTextures[gltfMat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
                size_t sampler  = gltfTextures[gltfMat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

                m->resources.albedoImage = images[img];
                m->resources.albedoImage->sampler = samplers[sampler];
                m->indices.albedo = static_cast<int32_t>(img);
            }

            materials.emplace_back(m);
        }
    }

    void Scene::loadImages(std::vector<fastgltf::Image> &gltfImages, fastgltf::Asset& asset, const std::string& folderPath) {
        /* 
         * TODO optimize image loading with multithreading. 
         * Currently throws Validation Error: [ UNASSIGNED-Threading-MultipleThreads-Write ]
         * object of type VkQueue is simultaneously used
         */
        //#pragma omp parallel for num_threads(16)
        //for (int i = 0; i < gltf.images.size(); i++) { 
        //    fastgltf::Image& image = gltf.images[i];
        
        for (fastgltf::Image& image : gltfImages) {
            std::shared_ptr<Image> img = loadImage(folderPath, asset, image);

            if (img) {
                img->name = image.name.c_str();
                img->sampler = DefaultObjects::getSamplerLinear(); // TODO get loaded sampler
                images.emplace_back(img);
                std::cout << img->name << std::endl;
            }
            else {
                images.emplace_back(DefaultObjects::getImageError());
                std::cout << "glTF failed to load texture " << image.name << std::endl;
            }
        }
    }

    std::shared_ptr<Image> Scene::loadImage(const std::string &folder, fastgltf::Asset& asset, fastgltf::Image& image) {
        std::shared_ptr<Image> newImage;
        int width, height, nrChannels;
        unsigned char* data = nullptr;

        std::visit(fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
                data = stbi_load((folder + path).c_str(), &width, &height, &nrChannels, 4);
            },
            [&](fastgltf::sources::Array& vector) {
                const unsigned char* imgBytes = reinterpret_cast<const unsigned char*>(vector.bytes.data());
                data = stbi_load_from_memory(imgBytes, static_cast<int>(vector.bytes.size()),
                &width, &height, &nrChannels, 4);
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                    [](auto& arg) {},
                    [&](fastgltf::sources::Array& vector) {
                        const unsigned char* imgBytes = reinterpret_cast<const unsigned char*>(vector.bytes.data()) + bufferView.byteOffset;
                        data = stbi_load_from_memory(imgBytes, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                    }},
                    buffer.data
                );
            },}, image.data
        );

        if (data) {
            newImage = extractImage(data, width, height, nrChannels);
            stbi_image_free(data);
        }

        if (newImage && newImage->handle != VK_NULL_HANDLE) {
            return newImage;
        } else {
            return {};
        }
    }

    std::shared_ptr<Image> Scene::extractImage(unsigned char* data, int width, int height, int channels) {
        VkImageUsageFlags imageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VkExtent3D imagesize;
        imagesize.width  = width;
        imagesize.height = height;
        imagesize.depth  = 1;

        VkFormat format = extractFormat(channels);
        return std::make_shared<Image>(device, imagesize, format, imageFlags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, data);
    }

    VkFormat Scene::extractFormat(int channels) {
        return VK_FORMAT_R8G8B8A8_UNORM;

        switch (channels) {
            case 1:
                return VK_FORMAT_R8_UNORM;
                break;
            case 2:
                return VK_FORMAT_R8G8_UNORM;
                break;
            case 3:
                return VK_FORMAT_R8G8B8_UNORM;
                break;
            case 4:
            default:
                return VK_FORMAT_R8G8B8A8_UNORM;
                break;
        }
    }

    void Scene::loadSamplers(const std::vector<fastgltf::Sampler> &gltfSamplers) {
        // load samplers
        for (const fastgltf::Sampler& sampler : gltfSamplers) {
            VkSamplerCreateInfo samplerCreateInfo = {};
            samplerCreateInfo.sType         = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.maxLod        = VK_LOD_CLAMP_NONE;
            samplerCreateInfo.minLod        = 0;
            samplerCreateInfo.magFilter     = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
            samplerCreateInfo.minFilter     = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
            samplerCreateInfo.mipmapMode    = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
            samplerCreateInfo.pNext         = nullptr;

            VkSampler newSampler;
            vkCreateSampler(device.handle(), &samplerCreateInfo, nullptr, &newSampler);
            samplers.push_back(newSampler);
        }
    }

    VkFilter Scene::extractFilter(fastgltf::Filter filter) {
        switch (filter) {
            // nearest samplers
            case fastgltf::Filter::Nearest:
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::NearestMipMapLinear:
                return VK_FILTER_NEAREST;

            // linear samplers
            case fastgltf::Filter::Linear:
            case fastgltf::Filter::LinearMipMapNearest:
            case fastgltf::Filter::LinearMipMapLinear:
            default:
                return VK_FILTER_LINEAR;
        }
    }

    VkSamplerMipmapMode Scene::extractMipmapMode(fastgltf::Filter filter) {
        switch (filter) {
            case fastgltf::Filter::NearestMipMapNearest:
            case fastgltf::Filter::LinearMipMapNearest:
                return VK_SAMPLER_MIPMAP_MODE_NEAREST;

            case fastgltf::Filter::NearestMipMapLinear:
            case fastgltf::Filter::LinearMipMapLinear:
            default:
                return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        }
    }

    Scene::~Scene() {
        for (VkSampler &sampler : samplers) {
            vkDestroySampler(device.handle(), sampler, nullptr);
        }
    }
}