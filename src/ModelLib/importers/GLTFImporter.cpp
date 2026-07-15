#include "ModelLib/importers/GLTFImporter.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/PBRMaterial.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#define TINYGLTF_IMPLEMENTATION
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <cstring>
#include <tiny_gltf.h>
#include <unordered_map>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Core/utils.hpp"
namespace engine {
    namespace {
        std::string getTexturePath(const tinygltf::Model& model, int textureIndex, const std::string& baseDir, const std::string& cacheDir) {
            if (textureIndex < 0 || std::cmp_greater_equal(textureIndex, model.textures.size())) {
                return "";
            }
            const tinygltf::Texture& texture = model.textures[textureIndex];
            if (texture.source < 0 || std::cmp_greater_equal(texture.source, model.images.size())) {
                return "";
            }
            const tinygltf::Image& image = model.images[texture.source];
            if (!image.uri.empty()) {
                if (image.uri.starts_with("data:")) {
                    std::string extension = ".png";
                    if (image.mimeType == "image/jpeg") {
                        extension = ".jpg";
                    } else if (image.mimeType == "image/png") {
                        extension = ".png";
                    }
                    std::string cachePath = cacheDir + "/texture_" + std::to_string(texture.source) + extension;
                    if (!std::filesystem::exists(cachePath)) {
                        std::filesystem::create_directories(cacheDir);
                        std::ofstream outFile(cachePath, std::ios::binary);
                        if (outFile.is_open()) {
                            outFile.write(reinterpret_cast<const char*>(image.image.data()), image.image.size());
                            outFile.close();
                            return cachePath;
                        }
                        std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write cached texture: " << cachePath << RESET << '\n';
                    } else {
                        return cachePath;
                    }
                    return "";
                }
                return baseDir + image.uri;
            }
            if (image.bufferView >= 0) {
                std::string extension = ".png";
                if (image.mimeType == "image/jpeg") {
                    extension = ".jpg";
                } else if (image.mimeType == "image/png") {
                    extension = ".png";
                }
                std::string cachePath = cacheDir + "/embedded_texture_" + std::to_string(texture.source) + extension;
                if (!std::filesystem::exists(cachePath)) {
                    std::filesystem::create_directories(cacheDir);
                    std::ofstream outFile(cachePath, std::ios::binary);
                    if (outFile.is_open()) {
                        outFile.write(reinterpret_cast<const char*>(image.image.data()), image.image.size());
                        outFile.close();
                        return cachePath;
                    }
                    std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write embedded texture: " << cachePath << RESET << '\n';
                } else {
                    return cachePath;
                }
                return "";
            }
            if (!image.image.empty()) {
                std::string extension = ".png";
                if (image.mimeType == "image/jpeg") {
                    extension = ".jpg";
                } else if (image.mimeType == "image/png") {
                    extension = ".png";
                }
                std::string cachePath = cacheDir + "/embedded_buffer_image_" + std::to_string(texture.source) + extension;
                if (!std::filesystem::exists(cachePath)) {
                    std::filesystem::create_directories(cacheDir);
                    std::ofstream outFile(cachePath, std::ios::binary);
                    if (outFile.is_open()) {
                        outFile.write(reinterpret_cast<const char*>(image.image.data()), image.image.size());
                        outFile.close();
                        return cachePath;
                    }
                    std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write image from buffer: " << cachePath << RESET << '\n';
                } else {
                    return cachePath;
                }
            }
            return "";
        }
    struct PrimitivePatch {
        std::vector<Model::Vertex> vertices;
        std::vector<uint32_t> positionIndices;
        uint32_t               indexCount = 0;
    };
    PrimitivePatch buildPrimitivePatch(const tinygltf::Model& model,
        const tinygltf::Primitive& p, const glm::mat4& xf, bool anim,
        int matId, bool hasMorph, float xM, float yM, float zM) {
        PrimitivePatch pp;
        const auto& pa = model.accessors[p.attributes.at("POSITION")];
        const auto* ps = reinterpret_cast<const float*>(&model.buffers[model.bufferViews[pa.bufferView].buffer].data[
            model.bufferViews[pa.bufferView].byteOffset + pa.byteOffset]);
        const float* ns = nullptr, *ts = nullptr;
        if (p.attributes.contains("NORMAL")) { const auto& na = model.accessors[p.attributes.at("NORMAL")];
            ns = reinterpret_cast<const float*>(&model.buffers[model.bufferViews[na.bufferView].buffer].data[
                model.bufferViews[na.bufferView].byteOffset + na.byteOffset]); }
        if (p.attributes.contains("TEXCOORD_0")) { const auto& ta = model.accessors[p.attributes.at("TEXCOORD_0")];
            ts = reinterpret_cast<const float*>(&model.buffers[model.bufferViews[ta.bufferView].buffer].data[
                model.bufferViews[ta.bufferView].byteOffset + ta.byteOffset]); }
        const auto& ia = model.accessors[p.indices];
        const uint8_t* id = &model.buffers[model.bufferViews[ia.bufferView].buffer].data[
            model.bufferViews[ia.bufferView].byteOffset + ia.byteOffset];
        pp.vertices.reserve(ia.count);
        if (hasMorph) pp.positionIndices.reserve(ia.count);
        glm::mat3 nm; if (!anim && ns) nm = glm::transpose(glm::inverse(glm::mat3(xf)));
        for (size_t i = 0; i < ia.count; i++) {
            uint32_t idx = 0;
            if (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) idx = ((const uint16_t*)id)[i];
            else if (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) idx = ((const uint32_t*)id)[i];
            else if (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) idx = id[i];
            Model::Vertex v{}; v.materialId = matId;
            if (anim) v.position = {xM*ps[idx*3], yM*ps[idx*3+1], zM*ps[idx*3+2]};
            else { auto tr = xf * glm::vec4(ps[idx*3], ps[idx*3+1], ps[idx*3+2], 1); v.position = {xM*tr.x, yM*tr.y, zM*tr.z}; }
            if (ns) { glm::vec3 wn = anim ? glm::vec3(ns[idx*3], ns[idx*3+1], ns[idx*3+2]) : glm::normalize(nm * glm::vec3(ns[idx*3], ns[idx*3+1], ns[idx*3+2]));
                v.normal = {xM*wn.x, yM*wn.y, zM*wn.z}; } else v.normal = {0,1,0};
            v.uv = ts ? glm::vec2(ts[idx*2], 1-ts[idx*2+1]) : glm::vec2(0);
            v.color = {1,1,1};
            pp.vertices.push_back(v);
            if (hasMorph) pp.positionIndices.push_back(idx);
        }
        pp.indexCount = (uint32_t)ia.count;
        return pp;
    }
    }  // namespace
    bool GLTFImporter::load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ) {
        tinygltf::Model    gltfModel;
        tinygltf::TinyGLTF loader;
        // Skip pixel decode during parse -- store raw encoded bytes instead.
        // This avoids stb_image decode during file load; textures are decoded
        // lazily by the texture system when first used.
        loader.SetImageLoader(
            [](tinygltf::Image* image, const int, std::string*, std::string*, int, int,
                const unsigned char* bytes, int size, void*) -> bool {
                image->image.assign(bytes, bytes + size);
                return true;
            },
            nullptr);
        std::string        err;
        std::string        warn;
        bool               ret = false;
        if (filepath.find(".glb") != std::string::npos) {
            ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
        } else {
            ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
        }
        if (!warn.empty()) {
            std::cout << YELLOW << "[GLTFImporter] Warning: " << RESET << warn << '\n';
        }
        if (!err.empty()) {
            std::cerr << RED << "[GLTFImporter] Error: " << RESET << err << '\n';
            return false;
        }
        if (!ret) {
            std::cerr << RED << "[GLTFImporter] Failed to load glTF file: " << RESET << filepath << '\n';
            return false;
        }
        std::cout << "[" << GREEN << "GLTFImporter" << RESET << "]: File loaded successfully" << '\n';
        bool const hasAnimations = !gltfModel.animations.empty();
        if (hasAnimations) {
            std::cout << YELLOW
                      << "[GLTFImporter] Model has animations - vertices will remain "
                         "in local space"
                      << RESET << '\n';
        }
        std::string const baseDir  = filepath.substr(0, filepath.find_last_of("/\\") + 1);
        std::string const cacheDir = baseDir + ".gltf_texture_cache";
        builder.vertices.clear();
        builder.indices.clear();
        builder.materials.clear();
        builder.subMeshes.clear();
        // Pre-calculate total vertex/index count to avoid vector reallocation during mesh loading.
        {
            size_t totalIndices = 0;
            std::function<void(int)> countNode = [&](int nodeIndex) {
                const auto& node = gltfModel.nodes[nodeIndex];
                if (node.mesh >= 0) {
                    const auto& mesh = gltfModel.meshes[node.mesh];
                    for (const auto& prim : mesh.primitives) {
                        if (prim.indices >= 0) {
                            totalIndices += gltfModel.accessors[prim.indices].count;
                        }
                    }
                }
                for (int const child : node.children) {
                    countNode(child);
                }
            };
            const auto& scene = gltfModel.scenes[gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0];
            for (int const nodeIndex : scene.nodes) {
                countNode(nodeIndex);
            }
            builder.vertices.reserve(totalIndices);
            builder.indices.reserve(totalIndices);
        }
        std::unordered_map<uint64_t, uint32_t> primitiveVertexOffsets;
        std::unordered_map<uint64_t, uint32_t> primitiveVertexCounts;
        std::unordered_map<uint32_t, uint32_t>    vertexToPositionIndex;
        loadMaterials(builder, gltfModel, baseDir, cacheDir);
        loadMeshes(builder, gltfModel, flipX, flipY, flipZ, primitiveVertexOffsets, primitiveVertexCounts, vertexToPositionIndex, hasAnimations);
        loadMorphTargets(builder, gltfModel, primitiveVertexOffsets, primitiveVertexCounts, vertexToPositionIndex);
        builder.primitiveVertexOffsets = primitiveVertexOffsets;
        builder.primitiveVertexCounts  = primitiveVertexCounts;
        loadAnimations(builder, gltfModel);
        loadLights(builder, gltfModel);
        std::cout << GREEN << "[GLTFImporter] Loaded " << builder.materials.size() << " materials, " << builder.subMeshes.size() << " sub-meshes" << RESET << '\n';
        builder.nodes.resize(gltfModel.nodes.size());
        for (size_t i = 0; i < gltfModel.nodes.size(); i++) {
            const auto& gltfNode = gltfModel.nodes[i];
            auto&       node     = builder.nodes[i];
            node.name            = gltfNode.name;
            node.mesh            = gltfNode.mesh;
            if (gltfNode.matrix.size() == 16) {
                node.matrix    = glm::make_mat4(gltfNode.matrix.data());
                node.hasMatrix = true;
            } else {
                if (gltfNode.translation.size() == 3) {
                    node.translation = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
                }
                if (gltfNode.rotation.size() == 4) {
                    node.rotation =
                        glm::quat(static_cast<float>(gltfNode.rotation[3]), static_cast<float>(gltfNode.rotation[0]), static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2]));
                }
                if (gltfNode.scale.size() == 3) {
                    node.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
                }
            }
            for (int const childIdx : gltfNode.children) {
                node.children.push_back(childIdx);
            }
            if (!gltfNode.weights.empty()) {
                node.morphWeights.resize(gltfNode.weights.size());
                for (size_t w = 0; w < gltfNode.weights.size(); w++) {
                    node.morphWeights[w] = static_cast<float>(gltfNode.weights[w]);
                }
            }
        }
        return true;
    }
    void GLTFImporter::loadMaterials(Model::Builder& builder, const tinygltf::Model& model, const std::string& baseDir, const std::string& cacheDir) {
        for (size_t i = 0; i < model.materials.size(); i++) {
            const auto&         gltfMat = model.materials[i];
            Model::MaterialInfo matInfo;
            matInfo.name                    = gltfMat.name;
            matInfo.materialId              = static_cast<int>(i);
            const auto& pbr                 = gltfMat.pbrMetallicRoughness;
            matInfo.pbrMaterial.doubleSided = gltfMat.doubleSided;
            matInfo.pbrMaterial.albedo      = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
            if (gltfMat.alphaMode == "MASK") {
                matInfo.pbrMaterial.alphaMode   = AlphaMode::Mask;
                matInfo.pbrMaterial.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
            } else if (gltfMat.alphaMode == "BLEND") {
                matInfo.pbrMaterial.alphaMode = AlphaMode::Blend;
            } else {
                matInfo.pbrMaterial.alphaMode = AlphaMode::Opaque;
            }
            matInfo.pbrMaterial.metallic  = (pbr.metallicFactor >= 0.0) ? static_cast<float>(pbr.metallicFactor) : 0.0f;
            matInfo.pbrMaterial.roughness = (pbr.roughnessFactor >= 0.0) ? static_cast<float>(pbr.roughnessFactor) : 1.0f;
            matInfo.pbrMaterial.ao        = 1.0f;
            if (pbr.baseColorTexture.index >= 0) {
                matInfo.diffuseTexPath = getTexturePath(model, pbr.baseColorTexture.index, baseDir, cacheDir);
            }
            if (gltfMat.normalTexture.index >= 0) {
                matInfo.normalTexPath = getTexturePath(model, gltfMat.normalTexture.index, baseDir, cacheDir);
            }
            if (pbr.metallicRoughnessTexture.index >= 0) {
                matInfo.roughnessTexPath                        = getTexturePath(model, pbr.metallicRoughnessTexture.index, baseDir, cacheDir);
                matInfo.pbrMaterial.useMetallicRoughnessTexture = true;
                if (gltfMat.occlusionTexture.index == pbr.metallicRoughnessTexture.index) {
                    matInfo.pbrMaterial.useOcclusionRoughnessMetallicTexture = true;
                }
            }
            if (gltfMat.occlusionTexture.index >= 0) {
                matInfo.aoTexPath = getTexturePath(model, gltfMat.occlusionTexture.index, baseDir, cacheDir);
            }
            matInfo.pbrMaterial.emissiveColor = glm::vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);
            if (gltfMat.emissiveTexture.index >= 0) {
                matInfo.emissiveTexPath = getTexturePath(model, gltfMat.emissiveTexture.index, baseDir, cacheDir);
            }
            if (gltfMat.extensions.contains("KHR_materials_pbrSpecularGlossiness")) {
                const auto& ext                                   = gltfMat.extensions.at("KHR_materials_pbrSpecularGlossiness");
                matInfo.pbrMaterial.useSpecularGlossinessWorkflow = true;
                if (ext.Has("diffuseFactor")) {
                    const auto& f              = ext.Get("diffuseFactor");
                    matInfo.pbrMaterial.albedo = glm::vec4(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble(), f.Get(3).GetNumberAsDouble());
                }
                if (ext.Has("specularFactor")) {
                    const auto& f                      = ext.Get("specularFactor");
                    matInfo.pbrMaterial.specularFactor = glm::vec3(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble());
                }
                if (ext.Has("glossinessFactor")) {
                    matInfo.pbrMaterial.glossinessFactor = static_cast<float>(ext.Get("glossinessFactor").GetNumberAsDouble());
                }
                if (ext.Has("diffuseTexture")) {
                    const auto& tex        = ext.Get("diffuseTexture");
                    int const   index      = tex.Get("index").GetNumberAsInt();
                    matInfo.diffuseTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
                if (ext.Has("specularGlossinessTexture")) {
                    const auto& tex                   = ext.Get("specularGlossinessTexture");
                    int const   index                 = tex.Get("index").GetNumberAsInt();
                    matInfo.specularGlossinessTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_emissive_strength")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_emissive_strength");
                if (ext.Has("emissiveStrength")) {
                    matInfo.pbrMaterial.emissiveStrength = static_cast<float>(ext.Get("emissiveStrength").GetNumberAsDouble());
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_transmission")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_transmission");
                if (ext.Has("transmissionFactor")) {
                    matInfo.pbrMaterial.transmission = static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble());
                }
                if (ext.Has("transmissionTexture")) {
                    const auto& tex             = ext.Get("transmissionTexture");
                    int const   index           = tex.Get("index").GetNumberAsInt();
                    matInfo.transmissionTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_ior")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_ior");
                if (ext.Has("ior")) {
                    matInfo.pbrMaterial.ior = static_cast<float>(ext.Get("ior").GetNumberAsDouble());
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_iridescence")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_iridescence");
                if (ext.Has("iridescenceFactor")) {
                    matInfo.pbrMaterial.iridescence = static_cast<float>(ext.Get("iridescenceFactor").GetNumberAsDouble());
                }
                if (ext.Has("iridescenceIor")) {
                    matInfo.pbrMaterial.iridescenceIOR = static_cast<float>(ext.Get("iridescenceIor").GetNumberAsDouble());
                }
                if (ext.Has("iridescenceThicknessMaximum")) {
                    matInfo.pbrMaterial.iridescenceThickness = static_cast<float>(ext.Get("iridescenceThicknessMaximum").GetNumberAsDouble());
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_clearcoat")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_clearcoat");
                if (ext.Has("clearcoatFactor")) {
                    matInfo.pbrMaterial.clearcoat = static_cast<float>(ext.Get("clearcoatFactor").GetNumberAsDouble());
                }
                if (ext.Has("clearcoatRoughnessFactor")) {
                    matInfo.pbrMaterial.clearcoatRoughness = static_cast<float>(ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble());
                }
                if (ext.Has("clearcoatTexture")) {
                    const auto& tex          = ext.Get("clearcoatTexture");
                    int const   index        = tex.Get("index").GetNumberAsInt();
                    matInfo.clearcoatTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
                if (ext.Has("clearcoatRoughnessTexture")) {
                    const auto& tex                   = ext.Get("clearcoatRoughnessTexture");
                    int const   index                 = tex.Get("index").GetNumberAsInt();
                    matInfo.clearcoatRoughnessTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
                if (ext.Has("clearcoatNormalTexture")) {
                    const auto& tex                = ext.Get("clearcoatNormalTexture");
                    int const   index              = tex.Get("index").GetNumberAsInt();
                    matInfo.clearcoatNormalTexPath = getTexturePath(model, index, baseDir, cacheDir);
                }
            }
            if (gltfMat.extensions.contains("KHR_materials_volume")) {
                const auto& ext = gltfMat.extensions.at("KHR_materials_volume");
                if (ext.Has("thicknessFactor")) {
                    matInfo.pbrMaterial.thickness = static_cast<float>(ext.Get("thicknessFactor").GetNumberAsDouble());
                }
                if (ext.Has("attenuationDistance")) {
                    matInfo.pbrMaterial.attenuationDistance = static_cast<float>(ext.Get("attenuationDistance").GetNumberAsDouble());
                }
                if (ext.Has("attenuationColor")) {
                    const auto& f                        = ext.Get("attenuationColor");
                    matInfo.pbrMaterial.attenuationColor = glm::vec3(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble());
                }
            }
            const tinygltf::ExtensionMap* textureExtensions = nullptr;
            if (gltfMat.normalTexture.index >= 0 && (static_cast<unsigned int>(gltfMat.normalTexture.extensions.contains("KHR_texture_transform")) != 0u)) {
                textureExtensions = &gltfMat.normalTexture.extensions;
            } else if (pbr.baseColorTexture.index >= 0 && (static_cast<unsigned int>(pbr.baseColorTexture.extensions.contains("KHR_texture_transform")) != 0u)) {
                textureExtensions = &pbr.baseColorTexture.extensions;
            } else if (pbr.metallicRoughnessTexture.index >= 0 && (static_cast<unsigned int>(pbr.metallicRoughnessTexture.extensions.contains("KHR_texture_transform")) != 0u)) {
                textureExtensions = &pbr.metallicRoughnessTexture.extensions;
            } else if (gltfMat.occlusionTexture.index >= 0 && (static_cast<unsigned int>(gltfMat.occlusionTexture.extensions.contains("KHR_texture_transform")) != 0u)) {
                textureExtensions = &gltfMat.occlusionTexture.extensions;
            }
            if (textureExtensions != nullptr) {
                const auto& ext = textureExtensions->at("KHR_texture_transform");
                if (ext.Has("scale")) {
                    const auto& scale = ext.Get("scale");
                    if (scale.IsArray() && scale.ArrayLen() >= 1) {
                        matInfo.pbrMaterial.uvScale = static_cast<float>(scale.Get(0).GetNumberAsDouble());
                    }
                }
            }
            builder.materials.push_back(matInfo);
            std::string alphaModeStr = "OPAQUE";
            if (matInfo.pbrMaterial.alphaMode == AlphaMode::Mask) {
                alphaModeStr = "MASK";
            } else if (matInfo.pbrMaterial.alphaMode == AlphaMode::Blend) {
                alphaModeStr = "BLEND";
            }
            std::cout << "[" << GREEN << " Material " << RESET << "] " << BLUE << matInfo.name << RESET << " -> PBR(albedo=" << matInfo.pbrMaterial.albedo.r << "," << matInfo.pbrMaterial.albedo.g << ","
                      << matInfo.pbrMaterial.albedo.b << ", metallic=" << matInfo.pbrMaterial.metallic << ", roughness=" << matInfo.pbrMaterial.roughness << ", alphaMode=" << alphaModeStr << ")" << '\n';
        }
    }
    glm::mat4 GLTFImporter::computeNodeTransform(const tinygltf::Node& node) {
        auto nodeTransform = glm::mat4(1.0f);
        if (node.matrix.size() == 16) {
            nodeTransform = glm::make_mat4(node.matrix.data());
        } else {
            if (node.translation.size() == 3) {
                nodeTransform = glm::translate(nodeTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::quat const q(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
                nodeTransform *= glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                nodeTransform = glm::scale(nodeTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }
        return nodeTransform;
    }
    void GLTFImporter::processMesh(Model::Builder&      builder,
        const tinygltf::Model&                          model,
        int                                             meshIndex,
        int                                             nodeIndex,
        const glm::mat4&                                globalTransform,
        std::unordered_map<int, std::vector<uint32_t>>& indicesByMaterial,
        std::unordered_map<uint64_t, uint32_t>&         primitiveVertexOffsets,
        std::unordered_map<uint64_t, uint32_t>&         primitiveVertexCounts,
        std::unordered_map<uint32_t, uint32_t>&         vertexToPositionIndex,
        bool                                            hasAnimations,
        float                                           xMultiplier,
        float                                           yMultiplier,
        float                                           zMultiplier) {
        // Deprecated.
    }
    void GLTFImporter::loadMeshes(Model::Builder&  builder,
        const tinygltf::Model&                     model,
        bool                                       flipX,
        bool                                       flipY,
        bool                                       flipZ,
        std::unordered_map<uint64_t, uint32_t>&    primitiveVertexOffsets,
        std::unordered_map<uint64_t, uint32_t>&    primitiveVertexCounts,
        std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex,
        bool                                       hasAnimations) {
        struct NM { int n,m; glm::mat4 x; };
        std::vector<NM> refs;
        std::function<void(int,glm::mat4)> cl = [&](int i, glm::mat4 p){
            const auto& nd = model.nodes[i]; glm::mat4 g = p * computeNodeTransform(nd);
            if (nd.mesh>=0) refs.push_back({i,nd.mesh,g});
            for (int c:nd.children) cl(c,g);
        };
        const auto& sn = model.scenes[model.defaultScene>=0?model.defaultScene:0];
        for (int n:sn.nodes) cl(n,glm::mat4(1));
        float xM=flipX?-1:1,yM=flipY?-1:1,zM=flipZ?-1:1;
        struct J{ int n,m; glm::mat4 x; std::vector<std::pair<int,const tinygltf::Primitive*>> pr; };
        std::vector<J> js; js.reserve(refs.size());
        for (auto& r:refs){
            J j; j.n=r.n; j.m=r.m; j.x=r.x;
            for (size_t pp=0;pp<model.meshes[r.m].primitives.size();++pp) j.pr.emplace_back((int)pp,&model.meshes[r.m].primitives[pp]);
            js.push_back(std::move(j));}
        std::vector<std::future<std::vector<PrimitivePatch>>> ff;
        for (auto& j:js) ff.push_back(std::async(std::launch::async,[&model,&j,hasAnimations,xM,yM,zM](){
            std::vector<PrimitivePatch> pp; pp.reserve(j.pr.size());
            for (auto& [_,pr]:j.pr) pp.push_back(buildPrimitivePatch(model,*pr,j.x,hasAnimations,pr->material,!pr->targets.empty(),xM,yM,zM));
            return pp;}));
        std::unordered_map<int,std::vector<uint32_t>> ibm;
        std::unordered_map<int,int> matToNode;  // material -> originating node index
        for (size_t ji=0;ji<js.size();++ji){
            auto pp=ff[ji].get(); auto& j=js[ji];
            builder.nodePrimitiveIndices[j.n].reserve(builder.nodePrimitiveIndices[j.n].size()+pp.size());
            for (size_t pi=0;pi<pp.size();++pi){
                auto& p=pp[pi]; int pri=j.pr[pi].first,mat=j.pr[pi].second->material;
                uint64_t k=((uint64_t)j.m<<32)|(uint32_t)pri; uint32_t o=(uint32_t)builder.vertices.size();
                primitiveVertexOffsets[k]=o; builder.nodePrimitiveIndices[j.n].push_back(pri);
                builder.vertices.insert(builder.vertices.end(),p.vertices.begin(),p.vertices.end());
                for (uint32_t i=0;i<p.indexCount;++i){uint32_t g=o+i;builder.indices.push_back(g);ibm[mat].push_back(g);}
                if (matToNode.find(mat) == matToNode.end()) matToNode[mat] = j.n;
                if (!p.positionIndices.empty()) for (uint32_t i=0;i<p.indexCount;++i) vertexToPositionIndex[o+i]=p.positionIndices[i];
                primitiveVertexCounts[k]=p.indexCount;}}
        uint32_t co=0;
        for (auto& [mi,ii]:ibm){if(!ii.empty()){Model::SubMesh sm;sm.materialId=mi;sm.indexOffset=co;sm.indexCount=(uint32_t)ii.size();sm.nodeIndex = matToNode.count(mi) ? matToNode[mi] : -1;builder.subMeshes.push_back(sm);co+=sm.indexCount;}}
        std::vector<uint32_t> gi; gi.reserve(builder.indices.size());
        for (auto& sm:builder.subMeshes){auto& ii=ibm[sm.materialId];gi.insert(gi.end(),ii.begin(),ii.end());}
        builder.indices=std::move(gi); std::cout<<GREEN<<"[GLTFImporter] Meshes processed"<<RESET<<"\n";
    }
    void GLTFImporter::loadMorphTargets(Model::Builder&  builder,
        const tinygltf::Model&                           gltfModel,
        const std::unordered_map<uint64_t, uint32_t>&    primitiveVertexOffsets,
        const std::unordered_map<uint64_t, uint32_t>&    primitiveVertexCounts,
        const std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex) {
        for (size_t meshIdx = 0; meshIdx < gltfModel.meshes.size(); meshIdx++) {
            const auto& gltfMesh = gltfModel.meshes[meshIdx];
            for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); primIdx++) {
                const auto& primitive = gltfMesh.primitives[primIdx];
                if (primitive.targets.empty())
                    continue;
                Model::MorphTargetSet morphSet;
                uint64_t const         key = (static_cast<uint64_t>(meshIdx) << 32) | static_cast<uint32_t>(primIdx);
                if (primitiveVertexOffsets.contains(key)) {
                    morphSet.vertexOffset = primitiveVertexOffsets.at(key);
                    morphSet.vertexCount  = static_cast<uint32_t>(primitiveVertexCounts.at(key));
                    morphSet.positionIndices.resize(morphSet.vertexCount);
                    for (uint32_t i = 0; i < morphSet.vertexCount; i++) {
                        uint32_t const vertexIdx    = morphSet.vertexOffset + i;
                        morphSet.positionIndices[i] = vertexToPositionIndex.at(vertexIdx);
                    }
                } else {
                    morphSet.vertexOffset = 0;
                    morphSet.vertexCount  = static_cast<uint32_t>(gltfModel.accessors[primitive.attributes.at("POSITION")].count);
                    std::cerr << RED << "[GLTFImporter] Warning: Could not find vertex offset for mesh " << meshIdx << " primitive " << primIdx << RESET << '\n';
                }
                if (!gltfMesh.weights.empty()) {
                    morphSet.weights.resize(gltfMesh.weights.size());
                    for (size_t w = 0; w < gltfMesh.weights.size(); w++) {
                        morphSet.weights[w] = static_cast<float>(gltfMesh.weights[w]);
                    }
                } else {
                    morphSet.weights.resize(primitive.targets.size(), 0.0f);
                }
                // Process each morph target's deltas in parallel (each reads from independent accessors).
                std::vector<std::future<Model::MorphTarget>> targetFutures;
                targetFutures.reserve(primitive.targets.size());
                for (const auto& target : primitive.targets) {
                    targetFutures.push_back(std::async(std::launch::async, [&gltfModel, &target]() {
                        Model::MorphTarget mt;
                        if (target.contains("POSITION")) {
                            const auto& posAccessor   = gltfModel.accessors[target.at("POSITION")];
                            const auto& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
                            const auto& posBuffer     = gltfModel.buffers[posBufferView.buffer];
                            const auto* positions     = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);
                            mt.positionDeltas.resize(posAccessor.count);
                            if (posAccessor.ByteStride(posBufferView) == 0 || posAccessor.ByteStride(posBufferView) == sizeof(glm::vec3)) {
                                std::memcpy(mt.positionDeltas.data(), positions, posAccessor.count * sizeof(glm::vec3));
                            } else {
                                for (size_t i = 0; i < posAccessor.count; i++) {
                                    mt.positionDeltas[i] = glm::vec3(positions[(i * 3) + 0], positions[(i * 3) + 1], positions[(i * 3) + 2]);
                                }
                            }
                        }
                        if (target.contains("NORMAL")) {
                            const auto& normAccessor   = gltfModel.accessors[target.at("NORMAL")];
                            const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
                            const auto& normBuffer     = gltfModel.buffers[normBufferView.buffer];
                            const auto* normals        = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
                            mt.normalDeltas.resize(normAccessor.count);
                            if (normAccessor.ByteStride(normBufferView) == 0 || normAccessor.ByteStride(normBufferView) == sizeof(glm::vec3)) {
                                std::memcpy(mt.normalDeltas.data(), normals, normAccessor.count * sizeof(glm::vec3));
                            } else {
                                for (size_t i = 0; i < normAccessor.count; i++) {
                                    mt.normalDeltas[i] = glm::vec3(normals[(i * 3) + 0], normals[(i * 3) + 1], normals[(i * 3) + 2]);
                                }
                            }
                        }
                        return mt;
                    }));
                }
                for (auto& f : targetFutures) {
                    morphSet.targets.push_back(f.get());
                }
                if (!morphSet.targets.empty()) {
                    builder.morphTargetSets.push_back(morphSet);
                    std::cout << GREEN << "[GLTFImporter] Loaded " << morphSet.targets.size() << " morph targets for mesh " << meshIdx << RESET << '\n';
                }
            }
        }
    }
    void GLTFImporter::loadAnimations(Model::Builder& builder, const tinygltf::Model& gltfModel) {
        for (const auto& gltfAnim : gltfModel.animations) {
            Model::Animation animation;
            animation.name = gltfAnim.name.empty() ? "animation_" + std::to_string(builder.animations.size()) : gltfAnim.name;
            for (const auto& gltfSampler : gltfAnim.samplers) {
                Model::AnimationSampler sampler;
                const auto&             timeAccessor   = gltfModel.accessors[gltfSampler.input];
                const auto&             timeBufferView = gltfModel.bufferViews[timeAccessor.bufferView];
                const auto&             timeBuffer     = gltfModel.buffers[timeBufferView.buffer];
                const auto*             times          = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);
                sampler.times.resize(timeAccessor.count);
                std::memcpy(sampler.times.data(), times, timeAccessor.count * sizeof(float));
                for (size_t i = 0; i < timeAccessor.count; i++) {
                    animation.duration = std::max(sampler.times[i], animation.duration);
                }
                const auto& outputAccessor   = gltfModel.accessors[gltfSampler.output];
                const auto& outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
                const auto& outputBuffer     = gltfModel.buffers[outputBufferView.buffer];
                const auto* outputs          = reinterpret_cast<const float*>(&outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);
                if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
                    sampler.translations.resize(outputAccessor.count);
                    sampler.scales.resize(outputAccessor.count);
                    if (outputAccessor.ByteStride(outputBufferView) == 0 || outputAccessor.ByteStride(outputBufferView) == sizeof(glm::vec3)) {
                        std::memcpy(sampler.translations.data(), outputs, outputAccessor.count * sizeof(glm::vec3));
                        std::memcpy(sampler.scales.data(), outputs, outputAccessor.count * sizeof(glm::vec3));
                    } else {
                        for (size_t i = 0; i < outputAccessor.count; i++) {
                            sampler.translations[i] = glm::vec3(outputs[(i * 3) + 0], outputs[(i * 3) + 1], outputs[(i * 3) + 2]);
                            sampler.scales[i]       = sampler.translations[i];
                        }
                    }
                } else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
                    sampler.rotations.resize(outputAccessor.count);
                    if (outputAccessor.ByteStride(outputBufferView) == 0 || outputAccessor.ByteStride(outputBufferView) == sizeof(glm::vec4)) {
                        std::memcpy(sampler.rotations.data(), outputs, outputAccessor.count * sizeof(glm::quat));
                    } else {
                        for (size_t i = 0; i < outputAccessor.count; i++) {
                            sampler.rotations[i] = glm::quat(outputs[(i * 4) + 3], outputs[(i * 4) + 0], outputs[(i * 4) + 1], outputs[(i * 4) + 2]);
                        }
                    }
                } else if (outputAccessor.type == TINYGLTF_TYPE_SCALAR) {
                    size_t const weightsPerFrame = outputAccessor.count / timeAccessor.count;
                    sampler.morphWeights.resize(timeAccessor.count);
                    for (size_t i = 0; i < timeAccessor.count; i++) {
                        sampler.morphWeights[i].resize(weightsPerFrame);
                        for (size_t w = 0; w < weightsPerFrame; w++) {
                            sampler.morphWeights[i][w] = outputs[(i * weightsPerFrame) + w];
                        }
                    }
                }
                if (gltfSampler.interpolation == "LINEAR") {
                    sampler.interpolation = Model::AnimationSampler::LINEAR;
                } else if (gltfSampler.interpolation == "STEP") {
                    sampler.interpolation = Model::AnimationSampler::STEP;
                } else if (gltfSampler.interpolation == "CUBICSPLINE") {
                    sampler.interpolation = Model::AnimationSampler::CUBICSPLINE;
                }
                animation.samplers.push_back(sampler);
            }
            for (const auto& gltfChannel : gltfAnim.channels) {
                Model::AnimationChannel channel;
                channel.samplerIndex = gltfChannel.sampler;
                channel.targetNode   = gltfChannel.target_node;
                if (gltfChannel.target_path == "translation") {
                    channel.path = Model::AnimationChannel::TRANSLATION;
                } else if (gltfChannel.target_path == "rotation") {
                    channel.path = Model::AnimationChannel::ROTATION;
                } else if (gltfChannel.target_path == "scale") {
                    channel.path = Model::AnimationChannel::SCALE;
                } else if (gltfChannel.target_path == "weights") {
                    channel.path = Model::AnimationChannel::WEIGHTS;
                    std::cout << GREEN << "[GLTFImporter] Found morph target weight animation channel" << RESET << '\n';
                } else {
                    continue;
                }
                animation.channels.push_back(channel);
            }
            if (animation.channels.empty()) {
                std::cout << YELLOW << "[GLTFImporter] Warning: Animation '" << animation.name << "' has no supported channels, skipping" << RESET << '\n';
                continue;
            }
            builder.animations.push_back(animation);
            std::cout << GREEN << "[GLTFImporter] Loaded animation: " << BLUE << animation.name << RESET << " (" << animation.duration << "s, " << animation.channels.size() << " channels)" << '\n';
        }
    }
    void GLTFImporter::loadLights(Model::Builder& builder, const tinygltf::Model& gltfModel) {
        bool const hasExtension = gltfModel.extensions.count("KHR_lights_punctual") > 0;
        if (!hasExtension || gltfModel.lights.empty()) {
            return;
        }
        std::cout << GREEN << "[GLTFImporter] Loading " << gltfModel.lights.size() << " KHR_lights_punctual lights" << RESET << '\n';
        std::unordered_map<int, int> nodeToLight;
        for (size_t nodeIdx = 0; nodeIdx < gltfModel.nodes.size(); ++nodeIdx) {
            auto it = gltfModel.nodes[nodeIdx].extensions.find("KHR_lights_punctual");
            if (it != gltfModel.nodes[nodeIdx].extensions.end()) {
                auto const& lightExt = it->second;
                if (lightExt.IsObject()) {
                    auto const& obj = lightExt.Get<std::map<std::string, tinygltf::Value>>();
                    auto        it2 = obj.find("light");
                    if (it2 != obj.end() && it2->second.IsInt()) {
                        nodeToLight[static_cast<int>(nodeIdx)] = static_cast<int>(it2->second.Get<int>());
                    }
                }
            }
        }
        for (size_t i = 0; i < gltfModel.lights.size(); ++i) {
            auto const&      gltfLight = gltfModel.lights[i];
            Model::LightInfo light;
            light.name = gltfLight.name.empty() ? "light_" + std::to_string(i) : gltfLight.name;
            if (gltfLight.type == "directional") {
                light.type = Model::LightType::Directional;
            } else if (gltfLight.type == "spot") {
                light.type = Model::LightType::Spot;
            } else {
                light.type = Model::LightType::Point;
            }
            if (gltfLight.color.size() >= 3) {
                light.color = glm::vec3(
                    static_cast<float>(gltfLight.color[0]),
                    static_cast<float>(gltfLight.color[1]),
                    static_cast<float>(gltfLight.color[2]));
            } else {
                light.color = glm::vec3(1.0f);
            }
            if (gltfLight.intensity <= 0.0) {
                if (gltfLight.type == "directional") {
                    light.intensity = 10.0f;
                } else if (gltfLight.type == "spot") {
                    light.intensity = 10.0f;
                } else {
                    light.intensity = 10.0f;
                }
                std::cout << YELLOW << "[GLTFImporter]   Light '" << light.name << "': intensity=0 -> " << light.intensity << " (default)" << RESET << '\n';
            } else {
                light.intensity = static_cast<float>(gltfLight.intensity);
            }
            light.range = static_cast<float>(gltfLight.range);
            if (gltfLight.type == "spot") {
                light.innerCutoffAngle = static_cast<float>(gltfLight.spot.innerConeAngle);
                light.outerCutoffAngle = static_cast<float>(gltfLight.spot.outerConeAngle);
            }
            for (auto const& [nodeIdx, lightIdx] : nodeToLight) {
                if (lightIdx == static_cast<int>(i)) {
                    light.nodeIndices.push_back(nodeIdx);
                }
            }
            builder.lights.push_back(light);
        }
        if (!nodeToLight.empty()) {
            for (auto const& [nodeIdx, lightIdx] : nodeToLight) {
                std::cout << GREEN << "[GLTFImporter] Node-to-light mapping:" << RESET << " Node -> " << nodeIdx << " -> Light '" << builder.lights[lightIdx].name << "'\n";
            }
        }
    }
}  // namespace engine