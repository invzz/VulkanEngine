#pragma once

#include <entt/entity/fwd.hpp>

namespace engine {

class Scene;
class Skybox;
struct SkyboxSettings;
struct ShadowSettings;

struct SceneRuntimeStateView {
  Scene* scene = nullptr;
  entt::entity* selectedEntity = nullptr;
  entt::entity* cameraEntity = nullptr;
  Skybox* skybox = nullptr;
  SkyboxSettings* skySettings = nullptr;
  ShadowSettings* shadowSettings = nullptr;

  /**
   * @brief Check if all required pointers are non-null.
   * Nullable fields (skybox, skySettings, shadowSettings) are excluded —
   * they may be null before a scene is loaded or settings are initialized.
   */
  [[nodiscard]] bool isValid() const {
    return scene != nullptr
        && selectedEntity != nullptr
        && cameraEntity != nullptr;
  }
};

}  // namespace engine
