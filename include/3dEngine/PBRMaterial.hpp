#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace engine {

  class Texture; // Forward declaration

  struct PBRMaterial
  {
    // Base PBR properties
    glm::vec3 albedo{1.0f, 1.0f, 1.0f};
    float     metallic{0.0f};
    float     roughness{0.5f};
    float     ao{1.0f};

    // Clearcoat layer (for car paint, lacquered surfaces)
    float clearcoat{0.0f};           // Clearcoat strength [0, 1]
    float clearcoatRoughness{0.03f}; // Clearcoat layer roughness (typically smooth)

    // Anisotropic reflections (for brushed metals, fabric)
    float anisotropic{0.0f};         // Anisotropy strength [0, 1]
    float anisotropicRotation{0.0f}; // Rotation of anisotropic direction [0, 1] (0 = tangent aligned)

    // Texture tiling
    float uvScale{1.0f}; // UV coordinate scale for texture tiling

    // Texture maps (optional - nullptr means use constant values above)
    std::shared_ptr<Texture> albedoMap;    // Base color texture (sRGB)
    std::shared_ptr<Texture> normalMap;    // Normal map (tangent space)
    std::shared_ptr<Texture> metallicMap;  // Metallic texture (linear)
    std::shared_ptr<Texture> roughnessMap; // Roughness texture (linear)
    std::shared_ptr<Texture> aoMap;        // Ambient occlusion texture (linear)

    // Helper methods to check if textures are present
    bool hasAlbedoMap() const { return albedoMap != nullptr; }
    bool hasNormalMap() const { return normalMap != nullptr; }
    bool hasMetallicMap() const { return metallicMap != nullptr; }
    bool hasRoughnessMap() const { return roughnessMap != nullptr; }
    bool hasAOMap() const { return aoMap != nullptr; }
  };

} // namespace engine
