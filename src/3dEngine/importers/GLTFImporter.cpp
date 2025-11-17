#include "3dEngine/importers/GLTFImporter.hpp"

#define TINYGLTF_IMPLEMENTATION
// Don't define STB_IMAGE_IMPLEMENTATION - it's already defined in Texture.cpp
// tinygltf will use the stb functions already linked
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <unordered_map>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

#include "3dEngine/ansi_colors.hpp"
#include "3dEngine/utils.hpp"

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

  // Helper function to get texture path from glTF, handling both URI and embedded images
  static std::string getTexturePath(const tinygltf::Model& model, int textureIndex, const std::string& baseDir, const std::string& cacheDir)
  {
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
    {
      return "";
    }

    const tinygltf::Texture& texture = model.textures[textureIndex];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
    {
      return "";
    }

    const tinygltf::Image& image = model.images[texture.source];

    // If image has a URI, it's an external file
    if (!image.uri.empty())
    {
      // Check if it's a data URI (base64 embedded)
      if (image.uri.find("data:") == 0)
      {
        // It's a data URI - tinygltf has already decoded it into image.image
        // We need to write it to a cache file
        std::string extension = ".png"; // Default to PNG
        if (image.mimeType == "image/jpeg")
          extension = ".jpg";
        else if (image.mimeType == "image/png")
          extension = ".png";

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
        else
        {
          std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write cached texture: " << cachePath << RESET << std::endl;
          return "";
        }
      }
      else
      {
        // Regular file URI - return path relative to base directory
        return baseDir + image.uri;
      }
    }
    // Image is embedded in a bufferView
    else if (image.bufferView >= 0)
    {
      // Image data is embedded in the glTF file
      // tinygltf has already loaded it into image.image
      std::string extension = ".png"; // Default to PNG
      if (image.mimeType == "image/jpeg")
        extension = ".jpg";
      else if (image.mimeType == "image/png")
        extension = ".png";

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
      else
      {
        std::cerr << YELLOW << "[GLTFImporter] Warning: Failed to write embedded texture: " << cachePath << RESET << std::endl;
        return "";
      }
    }

    return "";
  }

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
      std::cout << YELLOW << "[GLTFImporter] Warning: " << RESET << warn << std::endl;
    }

    if (!err.empty())
    {
      std::cerr << RED << "[GLTFImporter] Error: " << RESET << err << std::endl;
      return false;
    }

    if (!ret)
    {
      std::cerr << RED << "[GLTFImporter] Failed to load glTF file: " << RESET << filepath << std::endl;
      return false;
    }

    std::cout << "[" << GREEN << "GLTFImporter" << RESET << "]: File loaded successfully" << std::endl;

    // Check if model has animations (we'll skip baking transforms if it does)
    bool hasAnimations = !gltfModel.animations.empty();
    if (hasAnimations)
    {
      std::cout << YELLOW << "[GLTFImporter] Model has animations - vertices will remain in local space" << RESET << std::endl;
    }

    // Get base directory for texture paths
    std::string baseDir  = filepath.substr(0, filepath.find_last_of("/\\") + 1);
    std::string cacheDir = baseDir + ".gltf_texture_cache";

    // Flip multipliers
    float xMultiplier = flipX ? -1.0f : 1.0f;
    float yMultiplier = flipY ? -1.0f : 1.0f;
    float zMultiplier = flipZ ? -1.0f : 1.0f;

    builder.vertices.clear();
    builder.indices.clear();
    builder.materials.clear();
    builder.subMeshes.clear();

    // Load materials first
    for (size_t i = 0; i < gltfModel.materials.size(); i++)
    {
      const auto&         gltfMat = gltfModel.materials[i];
      Model::MaterialInfo matInfo;
      matInfo.name       = gltfMat.name;
      matInfo.materialId = static_cast<int>(i);

      // glTF uses PBR metallic-roughness workflow
      const auto& pbr = gltfMat.pbrMetallicRoughness;

      // Base color (albedo)
      matInfo.pbrMaterial.albedo = glm::vec3(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2]);

      // Metallic and roughness
      matInfo.pbrMaterial.metallic  = static_cast<float>(pbr.metallicFactor);
      matInfo.pbrMaterial.roughness = static_cast<float>(pbr.roughnessFactor);
      matInfo.pbrMaterial.ao        = 1.0f;

      // Extract texture paths (handles both external URIs and embedded images)
      if (pbr.baseColorTexture.index >= 0)
      {
        matInfo.diffuseTexPath = getTexturePath(gltfModel, pbr.baseColorTexture.index, baseDir, cacheDir);
      }

      if (gltfMat.normalTexture.index >= 0)
      {
        matInfo.normalTexPath = getTexturePath(gltfModel, gltfMat.normalTexture.index, baseDir, cacheDir);
      }

      if (pbr.metallicRoughnessTexture.index >= 0)
      {
        matInfo.roughnessTexPath = getTexturePath(gltfModel, pbr.metallicRoughnessTexture.index, baseDir, cacheDir);
      }

      if (gltfMat.occlusionTexture.index >= 0)
      {
        matInfo.aoTexPath = getTexturePath(gltfModel, gltfMat.occlusionTexture.index, baseDir, cacheDir);
      }

      builder.materials.push_back(matInfo);

      std::cout << "[" << GREEN << " Material " << RESET << "] " << BLUE << matInfo.name << RESET << " -> PBR(albedo=" << matInfo.pbrMaterial.albedo.r << ","
                << matInfo.pbrMaterial.albedo.g << "," << matInfo.pbrMaterial.albedo.b << ", metallic=" << matInfo.pbrMaterial.metallic
                << ", roughness=" << matInfo.pbrMaterial.roughness << ")" << std::endl;
    }

    // Process all meshes in the scene
    const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0];

    std::unordered_map<Model::Vertex, uint32_t>    uniqueVertices{};
    std::unordered_map<int, std::vector<uint32_t>> indicesByMaterial;

    // Lambda to process a node recursively
    std::function<void(int, const glm::mat4&)> processNode = [&](int nodeIndex, const glm::mat4& parentTransform) {
      const tinygltf::Node& node = gltfModel.nodes[nodeIndex];

      // Compute node's local transformation matrix
      glm::mat4 nodeTransform = glm::mat4(1.0f);

      if (node.matrix.size() == 16)
      {
        // Node has a transformation matrix
        nodeTransform = glm::make_mat4(node.matrix.data());
      }
      else
      {
        // Compute from TRS (Translation, Rotation, Scale)
        if (node.translation.size() == 3)
        {
          nodeTransform = glm::translate(nodeTransform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
        }

        if (node.rotation.size() == 4)
        {
          glm::quat q(static_cast<float>(node.rotation[3]),
                      static_cast<float>(node.rotation[0]),
                      static_cast<float>(node.rotation[1]),
                      static_cast<float>(node.rotation[2]));
          nodeTransform *= glm::mat4_cast(q);
        }

        if (node.scale.size() == 3)
        {
          nodeTransform = glm::scale(nodeTransform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
        }
      }

      // Combine with parent transformation
      glm::mat4 globalTransform = parentTransform * nodeTransform;

      // Process mesh if present
      if (node.mesh >= 0)
      {
        const tinygltf::Mesh& mesh = gltfModel.meshes[node.mesh];

        for (const auto& primitive : mesh.primitives)
        {
          int materialId = primitive.material;

          // Get accessors for vertex attributes
          const auto&  posAccessor   = gltfModel.accessors[primitive.attributes.at("POSITION")];
          const auto&  posBufferView = gltfModel.bufferViews[posAccessor.bufferView];
          const auto&  posBuffer     = gltfModel.buffers[posBufferView.buffer];
          const float* positions     = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset]);

          const float* normals   = nullptr;
          const float* texCoords = nullptr;

          if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
          {
            const auto& normAccessor   = gltfModel.accessors[primitive.attributes.at("NORMAL")];
            const auto& normBufferView = gltfModel.bufferViews[normAccessor.bufferView];
            const auto& normBuffer     = gltfModel.buffers[normBufferView.buffer];
            normals                    = reinterpret_cast<const float*>(&normBuffer.data[normBufferView.byteOffset + normAccessor.byteOffset]);
          }

          if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
          {
            const auto& uvAccessor   = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
            const auto& uvBufferView = gltfModel.bufferViews[uvAccessor.bufferView];
            const auto& uvBuffer     = gltfModel.buffers[uvBufferView.buffer];
            texCoords                = reinterpret_cast<const float*>(&uvBuffer.data[uvBufferView.byteOffset + uvAccessor.byteOffset]);
          }

          // Check if primitive has indices
          if (primitive.indices < 0)
          {
            // No indices - use direct vertex access (not commonly used, skip for now)
            std::cerr << YELLOW << "[GLTFImporter] Warning: Primitive without indices not supported yet" << RESET << std::endl;
            continue;
          }

          // Get indices
          const auto&    indAccessor   = gltfModel.accessors[primitive.indices];
          const auto&    indBufferView = gltfModel.bufferViews[indAccessor.bufferView];
          const auto&    indBuffer     = gltfModel.buffers[indBufferView.buffer];
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
              worldPos = glm::vec3(positions[index * 3 + 0], positions[index * 3 + 1], positions[index * 3 + 2]);
            }
            else
            {
              // Bake transform into vertices
              glm::vec4 localPos    = glm::vec4(positions[index * 3 + 0], positions[index * 3 + 1], positions[index * 3 + 2], 1.0f);
              glm::vec4 transformed = globalTransform * localPos;
              worldPos              = glm::vec3(transformed);
            }

            vertex.position = {
                    xMultiplier * worldPos.x,
                    yMultiplier * worldPos.y,
                    zMultiplier * worldPos.z,
            };

            // Normal - apply normal transformation only if no animations
            if (normals)
            {
              glm::vec3 worldNormal;
              if (hasAnimations)
              {
                // Keep normals in local space
                worldNormal = glm::vec3(normals[index * 3 + 0], normals[index * 3 + 1], normals[index * 3 + 2]);
              }
              else
              {
                // Transform normals
                glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));
                glm::vec3 localNormal  = glm::vec3(normals[index * 3 + 0], normals[index * 3 + 1], normals[index * 3 + 2]);
                worldNormal            = glm::normalize(normalMatrix * localNormal);
              }

              vertex.normal = {
                      xMultiplier * worldNormal.x,
                      yMultiplier * worldNormal.y,
                      zMultiplier * worldNormal.z,
              };
            }
            else
            {
              vertex.normal = {0.0f, 1.0f, 0.0f};
            }

            // Texture coordinates
            if (texCoords)
            {
              vertex.uv = {
                      texCoords[index * 2 + 0],
                      texCoords[index * 2 + 1],
              };
            }
            else
            {
              vertex.uv = {0.0f, 0.0f};
            }

            // Color (default to white)
            vertex.color = {1.0f, 1.0f, 1.0f};

            // Add to vertex buffer with deduplication
            if (uniqueVertices.count(vertex) == 0)
            {
              uniqueVertices[vertex] = static_cast<uint32_t>(builder.vertices.size());
              builder.vertices.push_back(vertex);
            }

            uint32_t vertexIndex = uniqueVertices[vertex];
            builder.indices.push_back(vertexIndex);
            indicesByMaterial[materialId].push_back(vertexIndex);
          }
        }
      }

      // Process children recursively
      for (int childIndex : node.children)
      {
        processNode(childIndex, globalTransform);
      }
    };

    // Process all root nodes
    for (int nodeIndex : scene.nodes)
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

    std::cout << GREEN << "[GLTFImporter] Loaded " << builder.materials.size() << " materials, " << builder.subMeshes.size() << " sub-meshes" << RESET
              << std::endl;

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
          node.rotation = glm::quat(static_cast<float>(gltfNode.rotation[3]),
                                    static_cast<float>(gltfNode.rotation[0]),
                                    static_cast<float>(gltfNode.rotation[1]),
                                    static_cast<float>(gltfNode.rotation[2]));
        }
        if (gltfNode.scale.size() == 3)
        {
          node.scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
        }
      }

      for (int childIdx : gltfNode.children)
      {
        node.children.push_back(childIdx);
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
        const auto&  timeAccessor   = gltfModel.accessors[gltfSampler.input];
        const auto&  timeBufferView = gltfModel.bufferViews[timeAccessor.bufferView];
        const auto&  timeBuffer     = gltfModel.buffers[timeBufferView.buffer];
        const float* times          = reinterpret_cast<const float*>(&timeBuffer.data[timeBufferView.byteOffset + timeAccessor.byteOffset]);

        sampler.times.resize(timeAccessor.count);
        for (size_t i = 0; i < timeAccessor.count; i++)
        {
          sampler.times[i] = times[i];
          if (sampler.times[i] > animation.duration)
          {
            animation.duration = sampler.times[i];
          }
        }

        // Get output values
        const auto&  outputAccessor   = gltfModel.accessors[gltfSampler.output];
        const auto&  outputBufferView = gltfModel.bufferViews[outputAccessor.bufferView];
        const auto&  outputBuffer     = gltfModel.buffers[outputBufferView.buffer];
        const float* outputs          = reinterpret_cast<const float*>(&outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset]);

        // Store values (type determined by channel target path)
        if (outputAccessor.type == TINYGLTF_TYPE_VEC3)
        {
          sampler.translations.resize(outputAccessor.count);
          sampler.scales.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.translations[i] = glm::vec3(outputs[i * 3 + 0], outputs[i * 3 + 1], outputs[i * 3 + 2]);
            sampler.scales[i]       = sampler.translations[i]; // Same storage
          }
        }
        else if (outputAccessor.type == TINYGLTF_TYPE_VEC4)
        {
          sampler.rotations.resize(outputAccessor.count);
          for (size_t i = 0; i < outputAccessor.count; i++)
          {
            sampler.rotations[i] = glm::quat(outputs[i * 4 + 3], outputs[i * 4 + 0], outputs[i * 4 + 1], outputs[i * 4 + 2]);
          }
        }

        // Interpolation type
        if (gltfSampler.interpolation == "LINEAR")
          sampler.interpolation = Model::AnimationSampler::LINEAR;
        else if (gltfSampler.interpolation == "STEP")
          sampler.interpolation = Model::AnimationSampler::STEP;
        else if (gltfSampler.interpolation == "CUBICSPLINE")
          sampler.interpolation = Model::AnimationSampler::CUBICSPLINE;

        animation.samplers.push_back(sampler);
      }

      // Load channels
      for (const auto& gltfChannel : gltfAnim.channels)
      {
        Model::AnimationChannel channel;
        channel.samplerIndex = gltfChannel.sampler;
        channel.targetNode   = gltfChannel.target_node;

        if (gltfChannel.target_path == "translation")
          channel.path = Model::AnimationChannel::TRANSLATION;
        else if (gltfChannel.target_path == "rotation")
          channel.path = Model::AnimationChannel::ROTATION;
        else if (gltfChannel.target_path == "scale")
          channel.path = Model::AnimationChannel::SCALE;
        else
        {
          // Skip unsupported paths (weights/morph targets, etc.)
          if (gltfChannel.target_path == "weights")
          {
            std::cout << YELLOW << "[GLTFImporter] Warning: Morph target animations (weights) not yet supported, skipping channel" << RESET << std::endl;
          }
          continue;
        }

        animation.channels.push_back(channel);
      }

      if (animation.channels.empty())
      {
        std::cout << YELLOW << "[GLTFImporter] Warning: Animation '" << animation.name << "' has no supported channels, skipping" << RESET << std::endl;
        continue;
      }

      builder.animations.push_back(animation);
      std::cout << GREEN << "[GLTFImporter] Loaded animation: " << BLUE << animation.name << RESET << " (" << animation.duration << "s, "
                << animation.channels.size() << " channels)" << std::endl;
    }

    return true;
  }

} // namespace engine
