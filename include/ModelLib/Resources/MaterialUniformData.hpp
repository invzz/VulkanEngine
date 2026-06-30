#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MATERIALUNIFORMDATA_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MATERIALUNIFORMDATA_HPP

#include <glm/glm.hpp>

namespace engine {

    struct MaterialUniformData {
        glm::vec4 albedo{1.0f};
        glm::vec4 emissiveInfo{0.0f, 0.0f, 0.0f, 1.0f};             // rgb: color, a: strength
        glm::vec4 specularGlossinessFactor{1.0f};                   // rgb: specular, a: glossiness
        glm::vec4 attenuationColorAndDist{1.0f, 1.0f, 1.0f, 1.0f};  // rgb: color, a: distance

        // Packed float parameters
        // Col 0: metallic, roughness, ao, isSelected
        // Col 1: clearcoat, clearcoatRoughness, anisotropic, anisotropicRotation
        // Col 2: transmission, ior, iridescence, iridescenceIOR
        // Col 3: iridescenceThickness, uvScale, alphaCutoff, thickness
        glm::mat4 params{0.0f};

        // Packed uint parameters
        // x: textureFlags, y: alphaMode, z: albedoIndex, w: normalIndex
        glm::uvec4 flagsAndIndices0{0};
        // x: metallicIndex, y: roughnessIndex, z: aoIndex, w: emissiveIndex
        glm::uvec4 indices1{0};
        // x: specularGlossinessIndex, y: useSpecularGlossiness, z: transmissionIndex, w: clearcoatIndex
        glm::uvec4 indices2{0};
        // x: clearcoatRoughnessIndex, y: clearcoatNormalIndex, z: pad, w: pad
        glm::uvec4 indices3{0};
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MATERIALUNIFORMDATA_HPP
