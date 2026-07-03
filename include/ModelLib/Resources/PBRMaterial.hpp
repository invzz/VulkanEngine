#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_PBRMATERIAL_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_PBRMATERIAL_HPP

#include <glm/glm.hpp>

#include <memory>

namespace engine {

    class Texture;

    enum class AlphaMode : std::uint8_t {
        Opaque,
        Mask,
        Blend
    };

    struct PBRMaterial {
        glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
        float     metallic{0.0f};
        float     roughness{0.5f};
        float     ao{1.0f};

        AlphaMode alphaMode{AlphaMode::Opaque};
        float     alphaCutoff{0.5f};
        bool      doubleSided{false};

        float clearcoat{0.0f};
        float clearcoatRoughness{0.03f};

        float anisotropic{0.0f};
        float anisotropicRotation{0.0f};

        float     transmission{0.0f};
        float     ior{1.5f};
        float     thickness{0.0f};
        glm::vec3 attenuationColor{1.0f, 1.0f, 1.0f};

        float attenuationDistance{1.0f};

        float iridescence{0.0f};
        float iridescenceIOR{1.3f};
        float iridescenceThickness{100.0f};

        glm::vec3 emissiveColor{0.0f};
        float     emissiveStrength{1.0f};

        bool useMetallicRoughnessTexture{false};

        bool useOcclusionRoughnessMetallicTexture{false};

        bool useSpecularGlossinessWorkflow{false};

        glm::vec3 specularFactor{1.0f};
        float     glossinessFactor{1.0f};

        float uvScale{1.0f};

        std::shared_ptr<Texture> albedoMap;
        std::shared_ptr<Texture> normalMap;
        std::shared_ptr<Texture> metallicMap;
        std::shared_ptr<Texture> roughnessMap;
        std::shared_ptr<Texture> aoMap;
        std::shared_ptr<Texture> emissiveMap;
        std::shared_ptr<Texture> specularGlossinessMap;
        std::shared_ptr<Texture> transmissionMap;
        std::shared_ptr<Texture> clearcoatMap;
        std::shared_ptr<Texture> clearcoatRoughnessMap;
        std::shared_ptr<Texture> clearcoatNormalMap;

        [[nodiscard]] bool hasAlbedoMap() const {
            return albedoMap != nullptr;
        }
        [[nodiscard]] bool hasNormalMap() const {
            return normalMap != nullptr;
        }
        [[nodiscard]] bool hasMetallicMap() const {
            return metallicMap != nullptr;
        }
        [[nodiscard]] bool hasRoughnessMap() const {
            return roughnessMap != nullptr;
        }
        [[nodiscard]] bool hasAOMap() const {
            return aoMap != nullptr;
        }
        [[nodiscard]] bool hasEmissiveMap() const {
            return emissiveMap != nullptr;
        }
        [[nodiscard]] bool hasTransmissionMap() const {
            return transmissionMap != nullptr;
        }
        [[nodiscard]] bool hasClearcoatMap() const {
            return clearcoatMap != nullptr;
        }
        [[nodiscard]] bool hasClearcoatRoughnessMap() const {
            return clearcoatRoughnessMap != nullptr;
        }
        [[nodiscard]] bool hasClearcoatNormalMap() const {
            return clearcoatNormalMap != nullptr;
        }
    };

}  // namespace engine

#endif
