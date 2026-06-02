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
};

}  // namespace engine
