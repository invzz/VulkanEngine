#include "EngineImporters/Resources/importers/GLTFImporter.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Resources/Model.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"

#define TINYGLTF_IMPLEMENTATION
// STB implementations provided by the dedicated 'stb_provider' target.
#include <tiny_gltf.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <unordered_map>

#include "Engine/Core/ansi_colors.hpp"
#include "Engine/Core/utils.hpp"

// Hash function for Model::Vertex
namespace std {
  template <> struct hash<engine::Model::Vertex>
  {
    size_t operator()(engine::Model::Vertex const& vertex) const
    {
      size_t seed = 0;
      engine::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
      return seed;
    }
  };
} // namespace std

namespace engine {

  namespace {
    // Helper function to get texture path from glTF, handling both URI and embedded
    // images
    std::string getTexturePath(const tinygltf::Model& model, int textureIndex, const std::string& baseDir, const std::string& cacheDir)
    {
      if (textureIndex < 0 || std::cmp_greater_equal(textureIndex, model.textures.size()))
      {
        return "";
      }

      const tinygltf::Texture& texture = model.textures[textureIndex];
      if (texture.source < 0 || std::cmp_greater_equal(texture.source, model.images.size()))
      {
        return "";
      }

      const tinygltf::Image& image = model.images[texture.source];

      // If image has a URI, it's an external file
      if (!image.uri.empty())
      {
        // Check if it's a data URI (base64 embedded)
        if (image.uri.starts_with("data:"))
        {
          // It's a data URI - tinygltf has already decoded it into image.image
          // We need to write it to a cache file
          std::string extension = ".png"; // Default to PNG
          if (image.mimeType == "image/jpeg")
          {
            extension = ".jpg";
          }
          else if (image.mimeType == "image/png")
          {
            extension = ".png";
          }

          std::string cachePath = cacheDir + "/texture_" + std::to_string(texture.source) + extension;

          // Create cache directory if it doesn't exist
          std::filesystem::create_directories(cacheDir);

          // Write the image data to file
          std::ofstream outFile(cachePath, std::ios::binary);
          if (outFile.is_open())
          {
            outFile.write(reinterpret_cast<const char*>(image.image.data()), image.image.size());
            outFile.close();
            return cachePath;
          }

          std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write cached texture: " << cachePath << RESET << '\n';
          return "";
        }

        // Regular file URI - return path relative to base directory
        return baseDir + image.uri;
      }
      // Image is embedded in a bufferView
      if (image.bufferView >= 0)
      {
        // Image data is embedded in the glTF file
        // tinygltf has already loaded it into image.image
        std::string extension = ".png"; // Default to PNG
        if (image.mimeType == "image/jpeg")
        {
          extension = ".jpg";
        }
        else if (image.mimeType == "image/png")
        {
          extension = ".png";
        }

        std::string cachePath = cacheDir + "/embedded_texture_" + std::to_string(texture.source) + extension;

        // Create cache directory if it doesn't exist
        std::filesystem::create_directories(cacheDir);

        // Write the image data to file
        std::ofstream outFile(cachePath, std::ios::binary);
        if (outFile.is_open())
        {
          outFile.write(reinterpret_cast<const char*>(image.image.data()), image.image.size());
          outFile.close();
          return cachePath;
        }

        std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write embedded texture: " << cachePath << RESET << '\n';
        return "";
      }

      return "";
    }
  } // namespace

  bool GLTFImporter::load(Model::Builder& builder, const std::string& filepath, bool flipX, bool flipY, bool flipZ)
  {
    tinygltf::Model    gltfModel;
    tinygltf::TinyGLTF loader;
    std::string        err;
    std::string        warn;

    // Determine file type and load
    bool ret = false;
    if (filepath.find(".glb") != std::string::npos)
    {
      ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
    }
    else
    {
      ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
    }

    if (!warn.empty())
    {
      std::cout << YELLOW << "[GLTFImporter] Warning: " << RESET << warn << '\n';
    }

    if (!err.empty())
    {
      std::cerr << RED << "[GLTFImporter] Error: " << RESET << err << '\n';
      return false;
    }

    if (!ret)
    {
      std::cerr << RED << "[GLTFImporter] Failed to load glTF file: " << RESET << filepath << '\n';
      return false;
    }

    std::cout << "[" << GREEN << "GLTFImporter" << RESET << "]: File loaded successfully" << '\n';

    // Check if model has animations (we'll skip baking transforms if it does)
    bool const hasAnimations = !gltfModel.animations.empty();
    if (hasAnimations)
    {
      std::cout << YELLOW
                << "[GLTFImporter] Model has animations - vertices will remain "
                   "in local space"
                << RESET << '\n';
    }

    // Get base directory for texture paths
    std::string const baseDir  = filepath.substr(0, filepath.find_last_of("/\\") + 1);
    std::string const cacheDir = baseDir + ".gltf_texture_cache";

    // Flip multipliers
    // float const xMultiplier = flipX ? -1.0f : 1.0f;
    // float const yMultiplier = flipY ? -1.0f : 1.0f;
    // float const zMultiplier = flipZ ? -1.0f : 1.0f;

    builder.vertices.clear();
    builder.indices.clear();
    builder.materials.clear();
    builder.subMeshes.clear();

    // Track vertex offsets and counts for each mesh primitive (for morph targets)
    // Key: "meshIndex_primitiveIndex", Value: vertex offset/count in
    // builder.vertices
    std::unordered_map<std::string, uint32_t> primitiveVertexOffsets;
    std::unordered_map<std::string, uint32_t> primitiveVertexCounts;
    // Map from builder vertex index to original glTF position index (for morph
    // targets)
    std::unordered_map<uint32_t, uint32_t> vertexToPositionIndex;

    // Load materials (extracted)
    loadMaterials(builder, gltfModel, baseDir, cacheDir);

    // Load meshes (handles node traversal, vertex/index creation, submesh
    // grouping)
    loadMeshes(builder, gltfModel, flipX, flipY, flipZ, primitiveVertexOffsets, primitiveVertexCounts, vertexToPositionIndex, hasAnimations);

    // Load morph targets (requires primitive offsets/counts created in
    // loadMeshes)
    loadMorphTargets(builder, gltfModel, primitiveVertexOffsets, primitiveVertexCounts, vertexToPositionIndex);

    // Load animations
    loadAnimations(builder, gltfModel);

    std::cout << GREEN << "[GLTFImporter] Loaded " << builder.materials.size() << " materials, " << builder.subMeshes.size() << " sub-meshes" << RESET << '\n';

    // Load nodes (store original transforms before animation)
    builder.nodes.resize(gltfModel.nodes.size());
    for (size_t i = 0; i < gltfModel.nodes.size(); i++)
    {
      const auto& gltfNode = gltfModel.nodes[i];
      auto&       node     = builder.nodes[i];

      node.name = gltfNode.name;
      node.mesh = gltfNode.mesh;

      if (gltfNode.matrix.size() == 16)
      {
        node.matrix = glm::make_mat4(gltfNode.matrix.data());
      }
      else
      {
        if (gltfNode.translation.size() == 3)
        {
          node.translation = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
        }
        if (gltfNode.rotation.size() == 4)
        {
          node.rotation =
                  glm::quat(static_cast<float>(gltfNode.rotation[3]), static_cast<float>(gltfNode.rotation[0]), static_cast<float>(gltfNode.rotation[1]), static_cast<float>(gltfNode.rotation[2]));
        }
        if (gltfNode.scale.size() == 3)
        {
          node.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
        }
      }

      for (int const childIdx : gltfNode.children)
      {
        node.children.push_back(childIdx);
      }

      // Load morph target weights if present
      if (!gltfNode.weights.empty())
      {
        node.morphWeights.resize(gltfNode.weights.size());
        for (size_t w = 0; w < gltfNode.weights.size(); w++)
        {
          node.morphWeights[w] = static_cast<float>(gltfNode.weights[w]);
        }
      }
    }

    // Load morph targets from meshes
    for (size_t meshIdx = 0; meshIdx < gltfModel.meshes.size(); meshIdx++)
    {
      const auto& gltfMesh = gltfModel.meshes[meshIdx];

      for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); primIdx++)
      {
        const auto& primitive = gltfMesh.primitives[primIdx];

        if (primitive.targets.empty())
        {
          continue; // No morph targets
        }

        Model::MorphTargetSet morphSet;

        // Get the vertex offset and count for this primitive
        std::string const key = std::to_string(meshIdx) + "_" + std::to_string(primIdx);
        std::cout << "[GLTFImporter] Looking up morph target key: " << key << '\n';
        if (primitiveVertexOffsets.contains(key))
        {
          morphSet.vertexOffset = primitiveVertexOffsets[key];
          morphSet.vertexCount  = static_cast<uint32_t>(primitiveVertexCounts[key]); // Use actual vertex count

          // Store position index mapping for morph targets
          morphSet.positionIndices.resize(morphSet.vertexCount);
          for (uint32_t i = 0; i < morphSet.vertexCount; i++)
          {
            uint32_t const vertexIdx    = morphSet.vertexOffset + i;
            morphSet.positionIndices[i] = vertexToPositionIndex[vertexIdx];
          }

          std::cout << "[GLTFImporter] Found vertex offset: " << morphSet.vertexOffset << ", count: " << morphSet.vertexCount << '\n';
        }
        else
        {
          morphSet.vertexOffset = 0;
          morphSet.vertexCount  = static_cast<uint32_t>(gltfModel.accessors[primitive.attributes.at("POSITION")].count);
          std::cerr << RED << "[GLTFImporter] Warning: Could not find vertex offset for mesh " << meshIdx << " primitive " << primIdx << RESET << '\n';
        }

        // Initialize weights from mesh or node
        if (!gltfMesh.weights.empty())
        {
          morphSet.weights.resize(gltfMesh.weights.size());
          for (size_t w = 0; w < gltfMesh.weights.size(); w++)
          {
            morphSet.weights[w] = static_cast<float>(gltfMesh.weights[w]);
          }
        }
        else
        {
          morphSet.weights.resize(primitive.targets.size(), 0.0f);
        }

        // Load each morph target
        for (const auto& target : primitive.targets)
        {
          Model::MorphTarget morphTarget;

          // Load position deltas
          if (target.contains("POSITION"))
          {
            const auto& posAccessor   = gltfModel.accessors[target.at("POSITION")];
            const auto& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
            const auto& posBuffer     = gltfModel.buffers[posBufferView.buffer];
            const auto* positions     = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

            morphTarget.positionDeltas.resize(posAccessor.count);
            for (size_t i = 0; i < posAccessor.count; i++)
            {
              morphTarget.positionDeltas[i] = glm::vec3(positions[(i * 3) + 0], positions[(i * 3) + 1], positions[(i * 3) + 2]);
            }
          }

          // Load normal deltas
          if (target.contains("NORMAL"))
          {
            const auto& normAccessor   = gltfModel.accessors[target.at("NORMAL")];
            const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
            const auto& normBuffer     = gltfModel.buffers[normBufferView.buffer];
            const auto* normals        = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);

            morphTarget.normalDeltas.resize(normAccessor.count);
            for (size_t i = 0; i < normAccessor.count; i++)
            {
              morphTarget.normalDeltas[i] = glm::vec3(normals[(i * 3) + 0], normals[(i * 3) + 1], normals[(i * 3) + 2]);
            }
          }

          morphSet.targets.push_back(morphTarget);
        }

        if (!morphSet.targets.empty())
        {
          builder.morphTargetSets.push_back(morphSet);
          std::cout << GREEN << "[GLTFImporter] Loaded " << morphSet.targets.size() << " morph targets for mesh " << meshIdx << RESET << '\n';
        }
      }
    }

    // Load animations
    for (const auto& gltfAnim : gltfModel.animations)
    {
      Model::Animation animation;
      animation.name = gltfAnim.name.empty() ? "animation_" + std::to_string(builder.animations.size()) : gltfAnim.name;

      // Load samplers
      for (const auto& gltfSampler : gltfAnim.samplers)
      {
        Model::AnimationSampler sampler;

        // Get time values
        const auto& timeAccessor   = gltfModel.accessors[gltfSampler.input];
        const auto& timeBufferView = gltfModel.bufferViews[timeAccessor.bufferView];
        const auto& timeBuffer     = gltfModel.buffers[timeBufferView.buffer];
        const auto* times          = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);

        sampler.times.resize(timeAccessor.count);
        for (size_t i = 0; i < timeAccessor.count; i++)
        {
          sampler.times[i]   = times[i];
          animation.duration = std::max(sampler.times[i], animation.duration);
        }

        // Get output values
        const auto& outputAccessor   = gltfModel.accessors[gltfSampler.output];
        const auto& outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
        const auto& outputBuffer     = gltfModel.buffers[outputBufferView.buffer];
        const auto* outputs          = reinterpret_cast<const float*>(&outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);

        // Store values (type determined by channel target path)
        if (outputAccessor.type == TINYGLTF_TYPE_VEC3)
        {
          sampler.translations.resize(outputAccessor.count);
          sampler.scales.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.translations[i] = glm::vec3(outputs[(i * 3) + 0], outputs[(i * 3) + 1], outputs[(i * 3) + 2]);
            sampler.scales[i]       = sampler.translations[i]; // Same storage
          }
        }
        else if (outputAccessor.type == TINYGLTF_TYPE_VEC4)
        {
          sampler.rotations.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.rotations[i] = glm::quat(outputs[(i * 4) + 3], outputs[(i * 4) + 0], outputs[(i * 4) + 1], outputs[(i * 4) + 2]);
          }
        }
        else if (outputAccessor.type == TINYGLTF_TYPE_SCALAR)
        {
          // Morph target weights - multiple scalars per keyframe
          // Count weights per keyframe by dividing total count by time count
          size_t const weightsPerFrame = outputAccessor.count / timeAccessor.count;
          sampler.morphWeights.resize(timeAccessor.count);

          for (size_t i = 0; i < timeAccessor.count; i++)
          {
            sampler.morphWeights[i].resize(weightsPerFrame);
            for (size_t w = 0; w < weightsPerFrame; w++)
            {
              sampler.morphWeights[i][w] = outputs[(i * weightsPerFrame) + w];
            }
          }
        }

        // Interpolation type
        if (gltfSampler.interpolation == "LINEAR")
        {
          sampler.interpolation = Model::AnimationSampler::LINEAR;
        }
        else if (gltfSampler.interpolation == "STEP")
        {
          sampler.interpolation = Model::AnimationSampler::STEP;
        }
        else if (gltfSampler.interpolation == "CUBICSPLINE")
        {
          sampler.interpolation = Model::AnimationSampler::CUBICSPLINE;
        }

        animation.samplers.push_back(sampler);
      }

      // Load channels
      for (const auto& gltfChannel : gltfAnim.channels)
      {
        Model::AnimationChannel channel;
        channel.samplerIndex = gltfChannel.sampler;
        channel.targetNode   = gltfChannel.target_node;

        if (gltfChannel.target_path == "translation")
        {
          channel.path = Model::AnimationChannel::TRANSLATION;
        }
        else if (gltfChannel.target_path == "rotation")
        {
          channel.path = Model::AnimationChannel::ROTATION;
        }
        else if (gltfChannel.target_path == "scale")
        {
          channel.path = Model::AnimationChannel::SCALE;
        }
        else if (gltfChannel.target_path == "weights")
        {
          channel.path = Model::AnimationChannel::WEIGHTS;
          std::cout << GREEN << "[GLTFImporter] Found morph target weight animation channel" << RESET << '\n';
        }
        else
        {
          // Skip unsupported paths
          continue;
        }

        animation.channels.push_back(channel);
      }

      if (animation.channels.empty())
      {
        std::cout << YELLOW << "[GLTFImporter] Warning: Animation '" << animation.name << "' has no supported channels, skipping" << RESET << '\n';
        continue;
      }

      builder.animations.push_back(animation);
      std::cout << GREEN << "[GLTFImporter] Loaded animation: " << BLUE << animation.name << RESET << " (" << animation.duration << "s, " << animation.channels.size() << " channels)" << '\n';
    }

    return true;
  }

  // --- Refactored helper method implementations ---

  void GLTFImporter::loadMaterials(Model::Builder& builder, const tinygltf::Model& model, const std::string& baseDir, const std::string& cacheDir)
  {
    for (size_t i = 0; i < model.materials.size(); i++)
    {
      const auto&         gltfMat = model.materials[i];
      Model::MaterialInfo matInfo;
      matInfo.name       = gltfMat.name;
      matInfo.materialId = static_cast<int>(i);

      // glTF uses PBR metallic-roughness workflow
      const auto& pbr = gltfMat.pbrMetallicRoughness;

      // Double Sided
      matInfo.pbrMaterial.doubleSided = gltfMat.doubleSided;

      // Base color (albedo)
      matInfo.pbrMaterial.albedo = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);

      // Alpha Mode
      if (gltfMat.alphaMode == "MASK")
      {
        matInfo.pbrMaterial.alphaMode   = AlphaMode::Mask;
        matInfo.pbrMaterial.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
      }
      else if (gltfMat.alphaMode == "BLEND")
      {
        matInfo.pbrMaterial.alphaMode = AlphaMode::Blend;
      }
      else
      {
        matInfo.pbrMaterial.alphaMode = AlphaMode::Opaque;
      }

      // Metallic and roughness
      // glTF spec defaults: metallicFactor = 0.0 (non-metal), roughnessFactor = 1.0 (fully rough).
      // Some loaders/structs may set different defaults internally, so prefer explicit fallbacks when value is not present.
      matInfo.pbrMaterial.metallic  = (pbr.metallicFactor >= 0.0) ? static_cast<float>(pbr.metallicFactor) : 0.0f;
      matInfo.pbrMaterial.roughness = (pbr.roughnessFactor >= 0.0) ? static_cast<float>(pbr.roughnessFactor) : 1.0f;

      matInfo.pbrMaterial.ao = 1.0f;

      // Extract texture paths (handles both external URIs and embedded images)
      if (pbr.baseColorTexture.index >= 0)
      {
        matInfo.diffuseTexPath = getTexturePath(model, pbr.baseColorTexture.index, baseDir, cacheDir);
      }

      if (gltfMat.normalTexture.index >= 0)
      {
        matInfo.normalTexPath = getTexturePath(model, gltfMat.normalTexture.index, baseDir, cacheDir);
      }

      if (pbr.metallicRoughnessTexture.index >= 0)
      {
        matInfo.roughnessTexPath                        = getTexturePath(model, pbr.metallicRoughnessTexture.index, baseDir, cacheDir);
        matInfo.pbrMaterial.useMetallicRoughnessTexture = true;

        if (gltfMat.occlusionTexture.index == pbr.metallicRoughnessTexture.index)
        {
          matInfo.pbrMaterial.useOcclusionRoughnessMetallicTexture = true;
        }
      }

      if (gltfMat.occlusionTexture.index >= 0)
      {
        matInfo.aoTexPath = getTexturePath(model, gltfMat.occlusionTexture.index, baseDir, cacheDir);
      }

      // Emissive Factor
      matInfo.pbrMaterial.emissiveColor = glm::vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);

      if (gltfMat.emissiveTexture.index >= 0)
      {
        matInfo.emissiveTexPath = getTexturePath(model, gltfMat.emissiveTexture.index, baseDir, cacheDir);
      }

      // Specular Glossiness Workflow
      if (gltfMat.extensions.contains("KHR_materials_pbrSpecularGlossiness"))
      {
        const auto& ext                                   = gltfMat.extensions.at("KHR_materials_pbrSpecularGlossiness");
        matInfo.pbrMaterial.useSpecularGlossinessWorkflow = true;

        if (ext.Has("diffuseFactor"))
        {
          const auto& f              = ext.Get("diffuseFactor");
          matInfo.pbrMaterial.albedo = glm::vec4(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble(), f.Get(3).GetNumberAsDouble());
        }

        if (ext.Has("specularFactor"))
        {
          const auto& f                      = ext.Get("specularFactor");
          matInfo.pbrMaterial.specularFactor = glm::vec3(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble());
        }

        if (ext.Has("glossinessFactor"))
        {
          matInfo.pbrMaterial.glossinessFactor = static_cast<float>(ext.Get("glossinessFactor").GetNumberAsDouble());
        }

        if (ext.Has("diffuseTexture"))
        {
          const auto& tex        = ext.Get("diffuseTexture");
          int const   index      = tex.Get("index").GetNumberAsInt();
          matInfo.diffuseTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }

        if (ext.Has("specularGlossinessTexture"))
        {
          const auto& tex                   = ext.Get("specularGlossinessTexture");
          int const   index                 = tex.Get("index").GetNumberAsInt();
          matInfo.specularGlossinessTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }
      }

      // Parse Extensions (emissive strength, transmission, ior, iridescence,
      // clearcoat, volume)
      if (gltfMat.extensions.contains("KHR_materials_emissive_strength"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_emissive_strength");
        if (ext.Has("emissiveStrength"))
        {
          matInfo.pbrMaterial.emissiveStrength = static_cast<float>(ext.Get("emissiveStrength").GetNumberAsDouble());
        }
      }

      if (gltfMat.extensions.contains("KHR_materials_transmission"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_transmission");
        if (ext.Has("transmissionFactor"))
        {
          matInfo.pbrMaterial.transmission = static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble());
        }
        if (ext.Has("transmissionTexture"))
        {
          const auto& tex             = ext.Get("transmissionTexture");
          int const   index           = tex.Get("index").GetNumberAsInt();
          matInfo.transmissionTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }
      }

      if (gltfMat.extensions.contains("KHR_materials_ior"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_ior");
        if (ext.Has("ior"))
        {
          matInfo.pbrMaterial.ior = static_cast<float>(ext.Get("ior").GetNumberAsDouble());
        }
      }

      if (gltfMat.extensions.contains("KHR_materials_iridescence"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_iridescence");
        if (ext.Has("iridescenceFactor"))
        {
          matInfo.pbrMaterial.iridescence = static_cast<float>(ext.Get("iridescenceFactor").GetNumberAsDouble());
        }
        if (ext.Has("iridescenceIor"))
        {
          matInfo.pbrMaterial.iridescenceIOR = static_cast<float>(ext.Get("iridescenceIor").GetNumberAsDouble());
        }
        if (ext.Has("iridescenceThicknessMaximum"))
        {
          matInfo.pbrMaterial.iridescenceThickness = static_cast<float>(ext.Get("iridescenceThicknessMaximum").GetNumberAsDouble());
        }
      }

      if (gltfMat.extensions.contains("KHR_materials_clearcoat"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_clearcoat");
        if (ext.Has("clearcoatFactor"))
        {
          matInfo.pbrMaterial.clearcoat = static_cast<float>(ext.Get("clearcoatFactor").GetNumberAsDouble());
        }
        if (ext.Has("clearcoatRoughnessFactor"))
        {
          matInfo.pbrMaterial.clearcoatRoughness = static_cast<float>(ext.Get("clearcoatRoughnessFactor").GetNumberAsDouble());
        }
        if (ext.Has("clearcoatTexture"))
        {
          const auto& tex          = ext.Get("clearcoatTexture");
          int const   index        = tex.Get("index").GetNumberAsInt();
          matInfo.clearcoatTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }
        if (ext.Has("clearcoatRoughnessTexture"))
        {
          const auto& tex                   = ext.Get("clearcoatRoughnessTexture");
          int const   index                 = tex.Get("index").GetNumberAsInt();
          matInfo.clearcoatRoughnessTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }
        if (ext.Has("clearcoatNormalTexture"))
        {
          const auto& tex                = ext.Get("clearcoatNormalTexture");
          int const   index              = tex.Get("index").GetNumberAsInt();
          matInfo.clearcoatNormalTexPath = getTexturePath(model, index, baseDir, cacheDir);
        }
      }

      if (gltfMat.extensions.contains("KHR_materials_volume"))
      {
        const auto& ext = gltfMat.extensions.at("KHR_materials_volume");
        if (ext.Has("thicknessFactor"))
        {
          matInfo.pbrMaterial.thickness = static_cast<float>(ext.Get("thicknessFactor").GetNumberAsDouble());
        }
        if (ext.Has("attenuationDistance"))
        {
          matInfo.pbrMaterial.attenuationDistance = static_cast<float>(ext.Get("attenuationDistance").GetNumberAsDouble());
        }
        if (ext.Has("attenuationColor"))
        {
          const auto& f                        = ext.Get("attenuationColor");
          matInfo.pbrMaterial.attenuationColor = glm::vec3(f.Get(0).GetNumberAsDouble(), f.Get(1).GetNumberAsDouble(), f.Get(2).GetNumberAsDouble());
        }
      }

      // Texture transform (KHR_texture_transform)
      const tinygltf::ExtensionMap* textureExtensions = nullptr;
      if (gltfMat.normalTexture.index >= 0 && (static_cast<unsigned int>(gltfMat.normalTexture.extensions.contains("KHR_texture_transform")) != 0u))
      {
        textureExtensions = &gltfMat.normalTexture.extensions;
      }
      else if (pbr.baseColorTexture.index >= 0 && (static_cast<unsigned int>(pbr.baseColorTexture.extensions.contains("KHR_texture_transform")) != 0u))
      {
        textureExtensions = &pbr.baseColorTexture.extensions;
      }
      else if (pbr.metallicRoughnessTexture.index >= 0 && (static_cast<unsigned int>(pbr.metallicRoughnessTexture.extensions.contains("KHR_texture_transform")) != 0u))
      {
        textureExtensions = &pbr.metallicRoughnessTexture.extensions;
      }
      else if (gltfMat.occlusionTexture.index >= 0 && (static_cast<unsigned int>(gltfMat.occlusionTexture.extensions.contains("KHR_texture_transform")) != 0u))
      {
        textureExtensions = &gltfMat.occlusionTexture.extensions;
      }

      if (textureExtensions != nullptr)
      {
        const auto& ext = textureExtensions->at("KHR_texture_transform");
        if (ext.Has("scale"))
        {
          const auto& scale = ext.Get("scale");
          if (scale.IsArray() && scale.ArrayLen() >= 1)
          {
            matInfo.pbrMaterial.uvScale = static_cast<float>(scale.Get(0).GetNumberAsDouble());
          }
        }
      }

      builder.materials.push_back(matInfo);

      std::string alphaModeStr = "OPAQUE";
      if (matInfo.pbrMaterial.alphaMode == AlphaMode::Mask)
      {
        alphaModeStr = "MASK";
      }
      else if (matInfo.pbrMaterial.alphaMode == AlphaMode::Blend)
      {
        alphaModeStr = "BLEND";
      }

      std::cout << "[" << GREEN << " Material " << RESET << "] " << BLUE << matInfo.name << RESET << " -> PBR(albedo=" << matInfo.pbrMaterial.albedo.r << "," << matInfo.pbrMaterial.albedo.g << ","
                << matInfo.pbrMaterial.albedo.b << ", metallic=" << matInfo.pbrMaterial.metallic << ", roughness=" << matInfo.pbrMaterial.roughness << ", alphaMode=" << alphaModeStr << ")" << '\n';
    }
  }

  glm::mat4 GLTFImporter::computeNodeTransform(const tinygltf::Node& node)
  {
    auto nodeTransform = glm::mat4(1.0f);

    if (node.matrix.size() == 16)
    {
      nodeTransform = glm::make_mat4(node.matrix.data());
    }
    else
    {
      if (node.translation.size() == 3)
      {
        nodeTransform = glm::translate(nodeTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
      }

      if (node.rotation.size() == 4)
      {
        glm::quat const q(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
        nodeTransform *= glm::mat4_cast(q);
      }

      if (node.scale.size() == 3)
      {
        nodeTransform = glm::scale(nodeTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
      }
    }

    return nodeTransform;
  }

  void GLTFImporter::processMesh(Model::Builder&                                 builder,
                                 const tinygltf::Model&                          model,
                                 int                                             meshIndex,
                                 const glm::mat4&                                globalTransform,
                                 std::unordered_map<Model::Vertex, uint32_t>&    uniqueVertices,
                                 std::unordered_map<int, std::vector<uint32_t>>& indicesByMaterial,
                                 std::unordered_map<std::string, uint32_t>&      primitiveVertexOffsets,
                                 std::unordered_map<std::string, uint32_t>&      primitiveVertexCounts,
                                 std::unordered_map<uint32_t, uint32_t>&         vertexToPositionIndex,
                                 bool                                            hasAnimations,
                                 float                                           xMultiplier,
                                 float                                           yMultiplier,
                                 float                                           zMultiplier)
  {
    const tinygltf::Mesh& mesh = model.meshes[meshIndex];

    int firstMaterial = -1;
    for (size_t primIdx = 0; primIdx < mesh.primitives.size(); primIdx++)
    {
      const auto& primitive = mesh.primitives[primIdx];

      if (firstMaterial < 0 && primitive.material >= 0)
      {
        firstMaterial = primitive.material;
      }

      // Record the starting vertex offset for this primitive (for morph targets)
      auto const        primitiveVertexOffset = static_cast<uint32_t>(builder.vertices.size());
      std::string const key                   = std::to_string(meshIndex) + "_" + std::to_string(primIdx);
      primitiveVertexOffsets[key]             = primitiveVertexOffset;
      // Check if this primitive has morph targets - if so, disable deduplication
      bool const hasMorphTargets = !primitive.targets.empty();

      int const   materialId    = primitive.material; // Get accessors for vertex attributes
      const auto& posAccessor   = model.accessors[primitive.attributes.at("POSITION")];
      const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
      const auto& posBuffer     = model.buffers[posBufferView.buffer];
      const auto* positions     = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

      const float* normals   = nullptr;
      const float* texCoords = nullptr;

      if (primitive.attributes.contains("NORMAL"))
      {
        const auto& normAccessor   = model.accessors[primitive.attributes.at("NORMAL")];
        const auto& normBufferView = model.bufferViews[normAccessor.bufferView];
        const auto& normBuffer     = model.buffers[normBufferView.buffer];
        normals                    = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
      }

      if (primitive.attributes.contains("TEXCOORD_0"))
      {
        const auto& uvAccessor   = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
        const auto& uvBuffer     = model.buffers[uvBufferView.buffer];
        texCoords                = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
      }

      // Check if primitive has indices
      if (primitive.indices < 0)
      {
        // No indices - use direct vertex access (not commonly used, skip for now)
        std::cerr << YELLOW
                  << "[GLTFImporter] Warning: Primitive without indices not "
                     "supported yet"
                  << RESET << '\n';
        continue;
      }

      // Get indices
      const auto&    indAccessor   = model.accessors[primitive.indices];
      const auto&    indBufferView = model.bufferViews[indAccessor.bufferView];
      const auto&    indBuffer     = model.buffers[indBufferView.buffer];
      const uint8_t* indData       = &indBuffer.data[indBufferView.byteOffset + indAccessor.byteOffset];

      // Process indices based on component type
      for (size_t i = 0; i < indAccessor.count; i++)
      {
        uint32_t index = 0;
        if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
        {
          index = reinterpret_cast<const uint16_t*>(indData)[i];
        }
        else if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
          index = reinterpret_cast<const uint32_t*>(indData)[i];
        }
        else if (indAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
        {
          index = indData[i];
        }

        Model::Vertex vertex{};
        vertex.materialId = materialId;

        // Position - apply node transformation only if no animations
        glm::vec3 worldPos;
        if (hasAnimations)
        {
          // Keep vertices in local space for animations
          worldPos = glm::vec3(positions[(index * 3) + 0], positions[(index * 3) + 1], positions[(index * 3) + 2]);
        }
        else
        {
          // Bake transform into vertices
          glm::vec4 const localPos    = glm::vec4(positions[(index * 3) + 0], positions[(index * 3) + 1], positions[(index * 3) + 2], 1.0f);
          glm::vec4 const transformed = globalTransform * localPos;
          worldPos                    = glm::vec3(transformed);
        }

        vertex.position = {xMultiplier * worldPos.x, yMultiplier * worldPos.y, zMultiplier * worldPos.z};

        // Normal - apply normal transformation only if no animations
        if (normals != nullptr)
        {
          glm::vec3 worldNormal;
          if (hasAnimations)
          {
            // Keep normals in local space
            worldNormal = glm::vec3(normals[(index * 3) + 0], normals[(index * 3) + 1], normals[(index * 3) + 2]);
          }
          else
          {
            // Transform normals
            glm::mat3 const normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));
            glm::vec3 const localNormal  = glm::vec3(normals[(index * 3) + 0], normals[(index * 3) + 1], normals[(index * 3) + 2]);
            worldNormal                  = glm::normalize(normalMatrix * localNormal);
          }

          vertex.normal = {xMultiplier * worldNormal.x, yMultiplier * worldNormal.y, zMultiplier * worldNormal.z};
        }
        else
        {
          vertex.normal = {0.0f, 1.0f, 0.0f};
        }

        // Texture coordinates
        if (texCoords != nullptr)
        {
          vertex.uv = {texCoords[(index * 2) + 0], 1.0f - texCoords[(index * 2) + 1]};
        }
        else
        {
          vertex.uv = {0.0f, 0.0f};
        }

        // Color (default to white)
        vertex.color = {1.0f, 1.0f, 1.0f};

        // Add to vertex buffer (disable deduplication for morph targets)
        if (hasMorphTargets)
        {
          // No deduplication - store mapping from vertex index to original glTF
          // position index
          auto const vertexIdx = static_cast<uint32_t>(builder.vertices.size());
          builder.indices.push_back(vertexIdx);
          builder.vertices.push_back(vertex);
          indicesByMaterial[materialId].push_back(vertexIdx);

          // Store mapping: builder vertex index -> original glTF position index
          vertexToPositionIndex[vertexIdx] = index;
        }
        else
        {
          // Normal deduplication for non-morph meshes
          if (!uniqueVertices.contains(vertex))
          {
            uniqueVertices[vertex] = static_cast<uint32_t>(builder.vertices.size());
            builder.vertices.push_back(vertex);
          }

          uint32_t const vertexIndex = uniqueVertices[vertex];
          builder.indices.push_back(vertexIndex);
          indicesByMaterial[materialId].push_back(vertexIndex);
        }
      }

      // Store the actual vertex count for this primitive (for morph targets)
      uint32_t const primitiveVertexCount = static_cast<uint32_t>(builder.vertices.size()) - primitiveVertexOffset;
      primitiveVertexCounts[key]          = primitiveVertexCount;
      std::cout << "[GLTFImporter] Mesh " << meshIndex << " prim " << primIdx << " added " << primitiveVertexCount << " vertices" << '\n';
    }
  }

  void GLTFImporter::loadMeshes(Model::Builder&                            builder,
                                const tinygltf::Model&                     model,
                                bool                                       flipX,
                                bool                                       flipY,
                                bool                                       flipZ,
                                std::unordered_map<std::string, uint32_t>& primitiveVertexOffsets,
                                std::unordered_map<std::string, uint32_t>& primitiveVertexCounts,
                                std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex,
                                bool                                       hasAnimations)
  {
    const tinygltf::Scene& scene = model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];

    std::unordered_map<Model::Vertex, uint32_t>    uniqueVertices{};
    std::unordered_map<int, std::vector<uint32_t>> indicesByMaterial;

    float xMultiplier = flipX ? -1.0f : 1.0f;
    float yMultiplier = flipY ? -1.0f : 1.0f;
    float zMultiplier = flipZ ? -1.0f : 1.0f;

    std::function<void(int, const glm::mat4&)> processNode = [&](int nodeIndex, const glm::mat4& parentTransform) {
      const tinygltf::Node& node            = model.nodes[nodeIndex];
      glm::mat4 const       nodeTransform   = computeNodeTransform(node);
      glm::mat4 const       globalTransform = parentTransform * nodeTransform;

      if (node.mesh >= 0)
      {
        processMesh(builder,
                    model,
                    node.mesh,
                    globalTransform,
                    uniqueVertices,
                    indicesByMaterial,
                    primitiveVertexOffsets,
                    primitiveVertexCounts,
                    vertexToPositionIndex,
                    hasAnimations,
                    xMultiplier,
                    yMultiplier,
                    zMultiplier);
      }

      for (int const childIndex : node.children)
      {
        processNode(childIndex, globalTransform);
      }
    };

    for (int const nodeIndex : scene.nodes)
    {
      processNode(nodeIndex, glm::mat4(1.0f));
    }

    // Create sub-meshes from grouped indices
    uint32_t currentOffset = 0;
    for (auto& [matId, matIndices] : indicesByMaterial)
    {
      if (!matIndices.empty())
      {
        Model::SubMesh subMesh;
        subMesh.materialId  = matId;
        subMesh.indexOffset = currentOffset;
        subMesh.indexCount  = static_cast<uint32_t>(matIndices.size());
        builder.subMeshes.push_back(subMesh);

        currentOffset += subMesh.indexCount;
      }
    }

    // Rebuild indices array grouped by material
    std::vector<uint32_t> groupedIndices;
    groupedIndices.reserve(builder.indices.size());

    for (const auto& subMesh : builder.subMeshes)
    {
      const auto& matIndices = indicesByMaterial[subMesh.materialId];
      groupedIndices.insert(groupedIndices.end(), matIndices.begin(), matIndices.end());
    }

    builder.indices = std::move(groupedIndices);

    std::cout << GREEN << "[GLTFImporter] Meshes processed" << RESET << '\n';
  }

  void GLTFImporter::loadMorphTargets(Model::Builder&                                  builder,
                                      const tinygltf::Model&                           gltfModel,
                                      const std::unordered_map<std::string, uint32_t>& primitiveVertexOffsets,
                                      const std::unordered_map<std::string, uint32_t>& primitiveVertexCounts,
                                      const std::unordered_map<uint32_t, uint32_t>&    vertexToPositionIndex)
  {
    for (size_t meshIdx = 0; meshIdx < gltfModel.meshes.size(); meshIdx++)
    {
      const auto& gltfMesh = gltfModel.meshes[meshIdx];

      for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); primIdx++)
      {
        const auto& primitive = gltfMesh.primitives[primIdx];

        if (primitive.targets.empty()) continue; // No morph targets

        Model::MorphTargetSet morphSet;

        // Get the vertex offset and count for this primitive
        std::string const key = std::to_string(meshIdx) + "_" + std::to_string(primIdx);
        if (primitiveVertexOffsets.contains(key))
        {
          morphSet.vertexOffset = primitiveVertexOffsets.at(key);
          morphSet.vertexCount  = static_cast<uint32_t>(primitiveVertexCounts.at(key));

          morphSet.positionIndices.resize(morphSet.vertexCount);
          for (uint32_t i = 0; i < morphSet.vertexCount; i++)
          {
            uint32_t const vertexIdx    = morphSet.vertexOffset + i;
            morphSet.positionIndices[i] = vertexToPositionIndex.at(vertexIdx);
          }
        }
        else
        {
          morphSet.vertexOffset = 0;
          morphSet.vertexCount  = static_cast<uint32_t>(gltfModel.accessors[primitive.attributes.at("POSITION")].count);
          std::cerr << RED << "[GLTFImporter] Warning: Could not find vertex offset for mesh " << meshIdx << " primitive " << primIdx << RESET << '\n';
        }

        // Initialize weights from mesh or node
        if (!gltfMesh.weights.empty())
        {
          morphSet.weights.resize(gltfMesh.weights.size());
          for (size_t w = 0; w < gltfMesh.weights.size(); w++)
          {
            morphSet.weights[w] = static_cast<float>(gltfMesh.weights[w]);
          }
        }
        else
        {
          morphSet.weights.resize(primitive.targets.size(), 0.0f);
        }

        // Load each morph target
        for (const auto& target : primitive.targets)
        {
          Model::MorphTarget morphTarget;

          // Load position deltas
          if (target.contains("POSITION"))
          {
            const auto& posAccessor   = gltfModel.accessors[target.at("POSITION")];
            const auto& posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
            const auto& posBuffer     = gltfModel.buffers[posBufferView.buffer];
            const auto* positions     = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

            morphTarget.positionDeltas.resize(posAccessor.count);
            for (size_t i = 0; i < posAccessor.count; i++)
            {
              morphTarget.positionDeltas[i] = glm::vec3(positions[(i * 3) + 0], positions[(i * 3) + 1], positions[(i * 3) + 2]);
            }
          }

          // Load normal deltas
          if (target.contains("NORMAL"))
          {
            const auto& normAccessor   = gltfModel.accessors[target.at("NORMAL")];
            const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
            const auto& normBuffer     = gltfModel.buffers[normBufferView.buffer];
            const auto* normals        = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);

            morphTarget.normalDeltas.resize(normAccessor.count);
            for (size_t i = 0; i < normAccessor.count; i++)
            {
              morphTarget.normalDeltas[i] = glm::vec3(normals[(i * 3) + 0], normals[(i * 3) + 1], normals[(i * 3) + 2]);
            }
          }

          morphSet.targets.push_back(morphTarget);
        }

        if (!morphSet.targets.empty())
        {
          builder.morphTargetSets.push_back(morphSet);
          std::cout << GREEN << "[GLTFImporter] Loaded " << morphSet.targets.size() << " morph targets for mesh " << meshIdx << RESET << '\n';
        }
      }
    }
  }

  void GLTFImporter::loadAnimations(Model::Builder& builder, const tinygltf::Model& gltfModel)
  {
    for (const auto& gltfAnim : gltfModel.animations)
    {
      Model::Animation animation;
      animation.name = gltfAnim.name.empty() ? "animation_" + std::to_string(builder.animations.size()) : gltfAnim.name;

      // Load samplers
      for (const auto& gltfSampler : gltfAnim.samplers)
      {
        Model::AnimationSampler sampler;

        // Get time values
        const auto& timeAccessor   = gltfModel.accessors[gltfSampler.input];
        const auto& timeBufferView = gltfModel.bufferViews[timeAccessor.bufferView];
        const auto& timeBuffer     = gltfModel.buffers[timeBufferView.buffer];
        const auto* times          = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);

        sampler.times.resize(timeAccessor.count);
        for (size_t i = 0; i < timeAccessor.count; i++)
        {
          sampler.times[i]   = times[i];
          animation.duration = std::max(sampler.times[i], animation.duration);
        }

        // Get output values
        const auto& outputAccessor   = gltfModel.accessors[gltfSampler.output];
        const auto& outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
        const auto& outputBuffer     = gltfModel.buffers[outputBufferView.buffer];
        const auto* outputs          = reinterpret_cast<const float*>(&outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);

        // Store values (type determined by channel target path)
        if (outputAccessor.type == TINYGLTF_TYPE_VEC3)
        {
          sampler.translations.resize(outputAccessor.count);
          sampler.scales.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.translations[i] = glm::vec3(outputs[(i * 3) + 0], outputs[(i * 3) + 1], outputs[(i * 3) + 2]);
            sampler.scales[i]       = sampler.translations[i]; // Same storage
          }
        }
        else if (outputAccessor.type == TINYGLTF_TYPE_VEC4)
        {
          sampler.rotations.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.rotations[i] = glm::quat(outputs[(i * 4) + 3], outputs[(i * 4) + 0], outputs[(i * 4) + 1], outputs[(i * 4) + 2]);
          }
        }
        else if (outputAccessor.type == TINYGLTF_TYPE_SCALAR)
        {
          // Morph target weights - multiple scalars per keyframe
          // Count weights per keyframe by dividing total count by time count
          size_t const weightsPerFrame = outputAccessor.count / timeAccessor.count;
          sampler.morphWeights.resize(timeAccessor.count);

          for (size_t i = 0; i < timeAccessor.count; i++)
          {
            sampler.morphWeights[i].resize(weightsPerFrame);
            for (size_t w = 0; w < weightsPerFrame; w++)
            {
              sampler.morphWeights[i][w] = outputs[(i * weightsPerFrame) + w];
            }
          }
        }

        // Interpolation type
        if (gltfSampler.interpolation == "LINEAR")
        {
          sampler.interpolation = Model::AnimationSampler::LINEAR;
        }
        else if (gltfSampler.interpolation == "STEP")
        {
          sampler.interpolation = Model::AnimationSampler::STEP;
        }
        else if (gltfSampler.interpolation == "CUBICSPLINE")
        {
          sampler.interpolation = Model::AnimationSampler::CUBICSPLINE;
        }

        animation.samplers.push_back(sampler);
      }

      // Load channels
      for (const auto& gltfChannel : gltfAnim.channels)
      {
        Model::AnimationChannel channel;
        channel.samplerIndex = gltfChannel.sampler;
        channel.targetNode   = gltfChannel.target_node;

        if (gltfChannel.target_path == "translation")
        {
          channel.path = Model::AnimationChannel::TRANSLATION;
        }
        else if (gltfChannel.target_path == "rotation")
        {
          channel.path = Model::AnimationChannel::ROTATION;
        }
        else if (gltfChannel.target_path == "scale")
        {
          channel.path = Model::AnimationChannel::SCALE;
        }
        else if (gltfChannel.target_path == "weights")
        {
          channel.path = Model::AnimationChannel::WEIGHTS;
          std::cout << GREEN << "[GLTFImporter] Found morph target weight animation channel" << RESET << '\n';
        }
        else
        {
          // Skip unsupported paths
          continue;
        }

        animation.channels.push_back(channel);
      }

      if (animation.channels.empty())
      {
        std::cout << YELLOW << "[GLTFImporter] Warning: Animation '" << animation.name << "' has no supported channels, skipping" << RESET << '\n';
        continue;
      }

      builder.animations.push_back(animation);
      std::cout << GREEN << "[GLTFImporter] Loaded animation: " << BLUE << animation.name << RESET << " (" << animation.duration << "s, " << animation.channels.size() << " channels)" << '\n';
    }
  }

} // namespace engine