#pragma once

#include "Engine/Application/Ports/ISceneAccessPort.hpp"

namespace engine {

class EngineState;

// Adapter that bridges EngineState to the scene access port.
class SceneAccessAdapter final : public ISceneAccessPort {
 public:
  explicit SceneAccessAdapter(EngineState& engineState);

  [[nodiscard]] Scene* scene() override;
  [[nodiscard]] Skybox* skybox() override;
  [[nodiscard]] SkyboxSettings* skySettings() override;
  [[nodiscard]] ShadowSettings* shadowSettings() override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
