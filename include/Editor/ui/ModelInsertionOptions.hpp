#pragma once

#include <cstdint>
#include <string>

namespace engine {

    /**
     * @brief Options for inserting a model into the scene.
     * Controls collider generation and other import settings.
     */
    struct ModelInsertionOptions {
        enum class StaticColliderImportMode : uint8_t {
            ForceOn,
            ForceOff,
            AutoDetect
        };

        StaticColliderImportMode staticColliderMode = StaticColliderImportMode::AutoDetect;
        bool                     enableTextures     = true;
        bool                     loadMaterials      = true;
        bool                     enableMorphTargets = false;
    };

}  // namespace engine
