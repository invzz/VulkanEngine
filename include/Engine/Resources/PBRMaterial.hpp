#pragma once

#include <glm/glm.hpp>
#include <memory>

#include "Engine/Scene/Component.hpp"

namespace engine {

  class Texture; // Forward declaration

  enum class AlphaMode
  {
    Opaque,
    Mask,
    Blend
  };

  struct PBRMaterial
  {
    // Base PBR properties
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    float     metallic{0.0f};
    float     roughness{0.5f};
    float     ao{1.0f};

    // Alpha Blending
    AlphaMode alphaMode{AlphaMode::Opaque};
    float     alphaCutoff{0.5f};

    // Clearcoat layer (for car paint, lacquered surfaces)
    float clearcoat{0.0f};           // Clearcoat strength [0, 1]
    float clearcoatRoughness{0.03f}; // Clearcoat layer roughness (typically smooth)

    // Anisotropic reflections (for brushed metals, fabric)
    float anisotropic{0.0f};         // Anisotropy strength [0, 1]
    float anisotropicRotation{0.0f}; // Rotation of anisotropic direction [0, 1] (0 = tangent aligned)

    // Transmission (Refraction/Transparency)
    float transmission{0.0f}; // Transmission factor [0, 1] (0 = opaque, 1 = fully transparent)
    float ior{1.5f};          // Index of Refraction (default 1.5)

    // Iridescence (Thin film interference)
    float iridescence{0.0f};            // Iridescence intensity [0, 1]
    float iridescenceIOR{1.3f};         // IOR of the thin film
    float iridescenceThickness{100.0f}; // Thickness of the thin film in nanometers (default 100nm)

    // Emissive
    glm::vec3 emissiveColor{0.0f};    // Emissive color (linear)
    float     emissiveStrength{1.0f}; // Emissive strength multiplier

    // Workflow
    bool useMetallicRoughnessTexture{false};          // If true, metallic/roughness are packed in roughnessMap (B/G channels)
    bool useOcclusionRoughnessMetallicTexture{false}; // If true, occlusion/roughness/metallic are packed in roughnessMap (R/G/B channels)
    bool useSpecularGlossinessWorkflow{false};        // If true, use KHR_materials_pbrSpecularGlossiness workflow

    // Specular Glossiness Workflow
    glm::vec3 specularFactor{1.0f};   // Specular color (F0)
    float     glossinessFactor{1.0f}; // Glossiness (1 - roughness)

    // Texture tiling
    float uvScale{1.0f}; // UV coordinate scale for texture tiling

    // Texture maps (optional - nullptr means use constant values above)
    std::shared_ptr<Texture> albedoMap;             // Base color texture (sRGB)
    std::shared_ptr<Texture> normalMap;             // Normal map (tangent space)
    std::shared_ptr<Texture> metallicMap;           // Metallic texture (linear)
    std::shared_ptr<Texture> roughnessMap;          // Roughness texture (linear)
    std::shared_ptr<Texture> aoMap;                 // Ambient occlusion texture (linear)
    std::shared_ptr<Texture> emissiveMap;           // Emissive texture (sRGB)
    std::shared_ptr<Texture> specularGlossinessMap; // Specular (RGB) + Glossiness (A) texture

    // Helper methods to check if textures are present
    bool hasAlbedoMap() const { return albedoMap != nullptr; }
    bool hasNormalMap() const { return normalMap != nullptr; }
    bool hasMetallicMap() const { return metallicMap != nullptr; }
    bool hasRoughnessMap() const { return roughnessMap != nullptr; }
    bool hasAOMap() const { return aoMap != nullptr; }
    bool hasEmissiveMap() const { return emissiveMap != nullptr; }
  };

} // namespace engine
