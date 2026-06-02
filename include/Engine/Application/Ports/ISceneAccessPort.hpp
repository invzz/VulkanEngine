#pragma once

namespace engine {

class Scene;
class Skybox;
class SkyboxSettings;
class ShadowSettings;

// Port for scene state access without knowing EngineState internals.
class ISceneAccessPort {
 public:
  virtual ~ISceneAccessPort() = default;

  [[nodiscard]] virtual Scene* scene() = 0;
  [[nodiscard]] virtual Skybox* skybox() = 0;
  [[nodiscard]] virtual SkyboxSettings* skySettings() = 0;
  [[nodiscard]] virtual ShadowSettings* shadowSettings() = 0;
};

}  // namespace engine
