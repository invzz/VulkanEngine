#ifndef VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MATERIALUNIFORMDATA_HPP
#define VULKANENGINE_INCLUDE_ENGINE_RESOURCES_MATERIALUNIFORMDATA_HPP
#include <glm/glm.hpp>
namespace engine {
    struct MaterialUniformData {
        glm::vec4  albedo{1.0f};
        glm::vec4  emissiveInfo{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4  specularGlossinessFactor{1.0f};
        glm::vec4  attenuationColorAndDist{1.0f, 1.0f, 1.0f, 1.0f};
        glm::mat4  params{0.0f};
        glm::uvec4 flagsAndIndices0{0};
        glm::uvec4 indices1{0};
        glm::uvec4 indices2{0};
        glm::uvec4 indices3{0};
    };
}  // namespace engine
#endif
