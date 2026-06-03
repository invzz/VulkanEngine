#pragma once

#include "Engine/Application/Ports/ISceneSettingsPort.hpp"

namespace engine {

class EngineState;

// Adapter that bridges EngineState to the scene settings port.
class SceneSettingsAdapter final : public ISceneSettingsPort {
 public:
  explicit SceneSettingsAdapter(EngineState& engineState);

  [[nodiscard]] SkyboxSettings* getSkySettings() override;
  [[nodiscard]] ShadowSettings* getShadowSettings() override;
  void reloadSkybox() override;
  void resetShadowSettings() override;
  void changeShadowSettings(bool enableShadowCulling,
                            float pointLightDefaultRange,
                            float spotLightDefaultRange) override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
