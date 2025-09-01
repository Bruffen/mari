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

    std::shared_ptr<Node> Scene::getNode(const uint32_t id) {
        for (auto& node : nodes) {
            if (node.second->getId() == id) {
                return node.second;
            }
        }

        return nullptr;
    }

    void Scene::addNode(std::shared_ptr<Node> node) {
        topNodes.emplace_back(node);
        nodes[node->name] = node;
    }

    Scene::Scene(Device &device, const std::string &path) : device{device} {
        constexpr auto gltfOptions = 
            fastgltf::Options::DontRequireValidAssetMember | 
            fastgltf::Options::AllowDouble | 
            fastgltf::Options::LoadExternalBuffers |
            fastgltf::Options::DecomposeNodeMatrices;
        constexpr auto parserOptions =
            fastgltf::Extensions::KHR_materials_emissive_strength |
            fastgltf::Extensions::KHR_materials_ior |
            fastgltf::Extensions::KHR_materials_volume |
            fastgltf::Extensions::KHR_lights_punctual;

        fastgltf::Parser parser{parserOptions};
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

        std::vector<std::shared_ptr<Node>> tmpNodes;
        std::vector<uint32_t>              tmpImageIds;

        loadSamplers(gltf.samplers);
        loadImages(gltf.images, gltf, folder); // TODO improve passing asset gltf
        loadMaterials(gltf.materials, gltf.textures);
        loadMeshes(gltf);
        loadCameras(gltf.cameras);
        // TODO there's a gltf.scenes

        // load all nodes and their meshes
        int nodeEmptyId = 0;
        for (fastgltf::Node& node : gltf.nodes) {
            std::shared_ptr<Node> newNode = std::make_shared<Node>();
            newNode->name = node.name.c_str();
            if (newNode->name == "") {
                newNode->name = "Node_" + std::to_string(nodeEmptyId++);
            }

            if (node.meshIndex.has_value()) {
                newNode->mesh = meshes[*node.meshIndex];
            }
            if (node.cameraIndex.has_value()) {
                newNode->camera = cameras[*node.cameraIndex];
                cameraObjects.emplace_back(newNode);
                newNode->isStatic = false; // TODO cameras could still be static, just switch to free camera on movement
            }

            tmpNodes.emplace_back(newNode);
            nodes[newNode->name] = newNode;

            std::visit(fastgltf::visitor { 
                [&](fastgltf::math::fmat4x4 matrix) {
                    memcpy(&newNode->worldMatrix, matrix.data(), sizeof(matrix));
                    assert(false && "TODO Currently this world matrix value will be overwritten");
                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 tl(transform.translation.x(), transform.translation.y(), transform.translation.z());
                    glm::quat rot(transform.rotation.w(), transform.rotation.x(), transform.rotation.y(), transform.rotation.z());
                    glm::vec3 sc(transform.scale.x(), transform.scale.y(), transform.scale.z());

                    newNode->transform.position = tl;
                    newNode->transform.rotation = glm::eulerAngles(rot);
                    newNode->transform.scale    = sc;
                } },
                node.transform
            );
        }

        // run loop again to setup transform hierarchy
        for (int i = 0; i < gltf.nodes.size(); i++) {
            fastgltf::Node& node = gltf.nodes[i];

            std::shared_ptr<Node>& sceneNode = tmpNodes[i];

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

    // Calculate matrices whether they're static nodes or not
    void Scene::start() {
        for (auto& g : topNodes) {
            initializeWorldMatrix(*g, transform.mat4());
        }
    }

    void Scene::initializeWorldMatrix(Node& g, const glm::mat4 &worldMatrix) {
        g.worldMatrix = worldMatrix * g.transform.mat4();
        for (auto& c : g.children) {
            initializeWorldMatrix(*c, g.worldMatrix);
        }
    }

    // Update non static nodes
    void Scene::update() {
        for (auto& g : topNodes) {
            g->update(transform.mat4());
        }
    }

    void Scene::loadCameras(const std::vector<fastgltf::Camera> &gltfCameras) {
        for (auto& camera : gltfCameras) {
            std::shared_ptr<Camera> newCamera = std::make_unique<Camera>();
            cameras.emplace_back(newCamera);
            std::visit(fastgltf::visitor {
                [&](const fastgltf::Camera::Perspective& perspective) {
                    newCamera->setPerspectiveProjection(
                        perspective.yfov, 
                        perspective.aspectRatio.value_or(16.0f/9.0f), // TODO aspect ratio will be overwritten by window's
                        perspective.znear, 
                        perspective.zfar.value_or(1000.0f)
                    );
                },
                [&](const fastgltf::Camera::Orthographic& orthographic) {
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
    }   

    void Scene::loadMeshes(const fastgltf::Asset &gltf) {

        std::vector<uint32_t> indices;
        std::vector<Mesh::Vertex> vertices;

        for (const fastgltf::Mesh& mesh : gltf.meshes) {
            std::shared_ptr<Mesh> newMesh = std::make_shared<Mesh>(device);
            newMesh->name = mesh.name;

            indices.clear();
            vertices.clear();

            for (const fastgltf::Primitive &p : mesh.primitives) {
                PrimMesh newPrimitive;
                newPrimitive.start = static_cast<uint32_t>(indices.size());
                newPrimitive.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count);

                uint32_t initialVertex = static_cast<uint32_t>(vertices.size());

                // load indexes
                {
                    const fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                    indices.reserve(indices.size() + indexaccessor.count);
                    
                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](std::uint32_t idx) {
                        indices.push_back(idx + initialVertex);
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
                            vertices[initialVertex + index] = newvtx;
                        }
                    );
                }

                // Tangents are calculated with MikkTSpace
                /*auto tangents = p.findAttribute("TANGENT");
                if (tangents != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[(*tangents).accessorIndex],
                        [&](fastgltf::math::fvec4 n, size_t index) {
                            vertices[initialVertex + index].tangent = {n.x(), n.y(), n.z(), n.w()};
                        });
                }*/

                // load vertex normals
                auto normals = p.findAttribute("NORMAL");
                if (normals != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[(*normals).accessorIndex],
                        [&](fastgltf::math::fvec3 n, size_t index) {
                            vertices[initialVertex + index].normal = {n.x(), n.y(), n.z()};
                        });
                }

                // load UVs
                auto uv = p.findAttribute("TEXCOORD_0");
                if (uv != p.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(gltf, gltf.accessors[(*uv).accessorIndex],
                        [&](fastgltf::math::fvec2 uv, size_t index) {
                            vertices[initialVertex + index].uv = {uv.x(), uv.y()};
                        });
                }

                // load vertex colors
                auto colors = p.findAttribute("COLOR_0");
                if (colors != p.attributes.end()) {
                    if (gltf.accessors[(*colors).accessorIndex].type == fastgltf::AccessorType::Vec3) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(gltf, gltf.accessors[(*colors).accessorIndex],
                            [&](fastgltf::math::fvec3 c, size_t index) {
                                vertices[initialVertex + index].color =  {c.x(), c.y(), c.z(), 1.0f};
                            });
                    }
                    if (gltf.accessors[(*colors).accessorIndex].type == fastgltf::AccessorType::Vec4) {
                        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(gltf, gltf.accessors[(*colors).accessorIndex],
                            [&](fastgltf::math::fvec4 c, size_t index) {
                                vertices[initialVertex + index].color =  {c.x(), c.y(), c.z(), c.w()};
                            });
                    }
                }

                if (p.materialIndex.has_value()) {
                    newPrimitive.material = materials[p.materialIndex.value()];
                    newPrimitive.material->index = static_cast<int32_t>(p.materialIndex.value());
                } else {
                    newPrimitive.material = materials[0];
                    newPrimitive.material->index = 0;
                }

                newMesh->primMeshes.push_back(newPrimitive);
            }

            newMesh->createIndexBuffers(indices);
            newMesh->createVertexBuffers(vertices);
            meshes.emplace_back(newMesh);
        }
    }


    void Scene::loadMaterials(const std::vector<fastgltf::Material> &gltfMaterials, const std::vector<fastgltf::Texture> &gltfTextures) {
        for (const fastgltf::Material& gltfMat : gltfMaterials) {
            std::shared_ptr<Material> m = std::make_shared<Material>();
            m->name                     = gltfMat.name.empty() ? "mari_unnamed_material" : gltfMat.name.c_str();
            
            if (gltfMat.alphaMode != fastgltf::AlphaMode::Opaque)  m->transparent = true;

            // PBR data
            m->data.constants.albedo.x  = gltfMat.pbrData.baseColorFactor[0];
            m->data.constants.albedo.y  = gltfMat.pbrData.baseColorFactor[1];
            m->data.constants.albedo.z  = gltfMat.pbrData.baseColorFactor[2];
            m->data.constants.albedo.w  = gltfMat.pbrData.baseColorFactor[3];
            m->data.constants.metallic  = gltfMat.pbrData.metallicFactor;
            m->data.constants.roughness = gltfMat.pbrData.roughnessFactor;

            if (gltfMat.pbrData.baseColorTexture.has_value()) {
                const fastgltf::Texture& texture = gltfTextures[gltfMat.pbrData.baseColorTexture.value().textureIndex];
                if (texture.imageIndex.has_value()) {
                    size_t img = texture.imageIndex.value();
                    m->textures.albedo = images[img];
                    m->data.indices.albedo = static_cast<int32_t>(img);
                }
                if (texture.samplerIndex.has_value()) {
                    m->textures.albedo->sampler = samplers[texture.samplerIndex.value()];
                }
            }

            if (gltfMat.pbrData.metallicRoughnessTexture.has_value()) {
                const fastgltf::Texture& texture = gltfTextures[gltfMat.pbrData.metallicRoughnessTexture.value().textureIndex];
                if (texture.imageIndex.has_value()) {
                    size_t img = texture.imageIndex.value();
                    m->textures.metallicRoughness = images[img];
                    m->data.indices.metallicRoughness = static_cast<int32_t>(img);
                }
                if (texture.samplerIndex.has_value()) {
                    m->textures.metallicRoughness->sampler = samplers[texture.samplerIndex.value()];
                }
            }

            // Normal
            // TODO Normal maps have a scale value
            if (gltfMat.normalTexture.has_value()) { 
                const fastgltf::Texture& texture = gltfTextures[gltfMat.normalTexture.value().textureIndex];
                if (texture.imageIndex.has_value()) {
                    size_t img = texture.imageIndex.value();
                    m->textures.normal = images[img];
                    m->data.indices.normal = static_cast<int32_t>(img);
                }
                if (texture.samplerIndex.has_value()) {
                    m->textures.normal->sampler = samplers[texture.samplerIndex.value()];
                }
            }
            
            // Emission 
            m->data.constants.emission = glm::vec4(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2], gltfMat.emissiveStrength);

            if (gltfMat.emissiveTexture.has_value()) {
                const fastgltf::Texture& texture = gltfTextures[gltfMat.emissiveTexture.value().textureIndex];
                if (texture.imageIndex.has_value()) {
                    size_t img = texture.imageIndex.value();
                    m->textures.emissive = images[img];
                    m->data.indices.emissive = static_cast<int32_t>(img);

                    // We multiply emission color by emissive texture,
                    // therefore, when there's a texture, set emission color to 1 if it's 0.
                    if (glm::length(glm::vec3(m->data.constants.emission)) <= 0.0f) {
                        m->data.constants.emission = glm::vec4{1.0f, 1.0f, 1.0f, m->data.constants.emission.a};
                    }
                }
                if (texture.samplerIndex.has_value()) {
                    m->textures.emissive->sampler = samplers[texture.samplerIndex.value()];
                }
            }

            // KHR_materials_ior
            if (gltfMat.ior != 1.5f) {
                m->data.constants.ior = gltfMat.ior;
            }

            // KHR_materials_volume
            if (gltfMat.volume) {
                // Check material.hpp for info
                m->data.constants.thickness = 1.0; // gltfMat.volume->thicknessFactor; 

                /* if (gltfMat.volume->thicknessTexture.has_value()) {
                    const fastgltf::Texture& texture = gltfTextures[gltfMat.volume->thicknessTexture.value().textureIndex];
                    if (texture.imageIndex.has_value()) {
                        size_t img = texture.imageIndex.value();
                        m->textures.thickness = images[img];
                        m->data.indices.thickness = static_cast<int32_t>(img);
                    }
                } */
            }

            materials.emplace_back(m);
        }

        // If there are no materials in scene, add a default one to avoid problems
        if (materials.empty()) {
            std::shared_ptr<Material> m = std::make_shared<Material>();
            m->name = "mari_default";

            m->data.constants.albedo.x  = 1.0f;
            m->data.constants.albedo.y  = 1.0f;
            m->data.constants.albedo.z  = 1.0f;
            m->data.constants.albedo.w  = 0.0f;
            m->data.constants.metallic  = 0.0f;
            m->data.constants.roughness = 1.0f;
            materials.emplace_back(m);
        }

        std::vector<MaterialData> materialDatas;
        materialDatas.reserve(materials.size());
        
        for (const auto& mat : materials) {
            materialDatas.push_back(mat->data);
        }
        
        VkDeviceSize materialBufferSize = sizeof(MaterialData) * materials.size();
        materialDataBuffer = std::make_unique<Buffer>(            
            device,
            sizeof(MaterialData),
            static_cast<uint32_t>(materials.size()),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        materialDataBuffer->stageToBuffer((void*) materialDatas.data());
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
                img->name = image.name.empty() ? "mari_unnamed_texture" : image.name.c_str();
                images.emplace_back(img);
                std::cout << img->name << std::endl;
            }
            else {
                images.emplace_back(DefaultObjects::getImageWhite());
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
                assert(filePath.uri.isLocalPath());   // We're only capable of loading local files.

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
                newImage = loadImage((folder + path).c_str(), VK_FORMAT_R8G8B8A8_UNORM);
            },
            [&](fastgltf::sources::Array& vector) {
                const unsigned char* imgBytes = reinterpret_cast<const unsigned char*>(vector.bytes.data());
                data = stbi_load_from_memory(imgBytes, static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
                newImage = extractImage(data, VK_FORMAT_R8G8B8A8_UNORM, width, height);
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                    [](auto& arg) {},
                    [&](fastgltf::sources::Array& vector) {
                        const unsigned char* imgBytes = reinterpret_cast<const unsigned char*>(vector.bytes.data()) + bufferView.byteOffset;
                        data = stbi_load_from_memory(imgBytes, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                        newImage = extractImage(data, VK_FORMAT_R8G8B8A8_UNORM, width, height);
                    }},
                    buffer.data
                );
            },}, image.data
        );

        return newImage;
    }

    std::shared_ptr<Image> Scene::extractImage(void* data, VkFormat format, int width, int height) {
        assert(data && "Null pointer to image data");
        std::shared_ptr<Image> image;

        if (data) {
            image = std::make_shared<Image>(
                device, 
                VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                format, 
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
                data
            );
            stbi_image_free(data);
        }

        if (image && image->handle != VK_NULL_HANDLE) {
            return image;
        } else {
            return {};
        }
    }

    std::shared_ptr<Image> Scene::loadImage(const std::string &path, VkFormat format) {
        assert(format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R32G32B32A32_SFLOAT && "Only R8G8B8A8_UNORM and R32G32B32A32_SFLOAT are implemented");

        int width, height, nrChannels;
        void* data = nullptr;

        if (format == VK_FORMAT_R32G32B32A32_SFLOAT) {
            data = stbi_loadf(path.c_str(), &width, &height, &nrChannels, 4);
        }
        else if (format == VK_FORMAT_R8G8B8A8_UNORM) {
            data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
        }

        if (data) {
            return extractImage(data, format, width, height);
        }
        throw std::runtime_error("Failed to load " + path);
        return {};
    }

    std::shared_ptr<Image> Scene::loadImage(const std::string &path, VkFormat format, const std::string &name) {
        std::shared_ptr<Image> img = loadImage(path, format);
        if (img) {
            img->name = name;
        }
        return img;
    }

    VkFormat Scene::extractFormat(int channels) {
        return VK_FORMAT_R8G8B8A8_UNORM; // TODO

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