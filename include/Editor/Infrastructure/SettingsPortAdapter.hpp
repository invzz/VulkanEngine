#pragma once

#include "Engine/Application/Ports/ISettingsPort.hpp"

namespace engine {

class EngineState;
class SkyboxSettings;
class PostProcessPushConstants;
class RenderingStateService;
class ResourceManager;
class ModelRenderSystem;

// Adapter that bridges EngineState to the settings port.
class SettingsPortAdapter final : public ISettingsPort {
 public:
  explicit SettingsPortAdapter(EngineState& engineState);

  // Settings
  void setShowSkybox(bool enabled) override;
  [[nodiscard]] bool showSkybox() const override;

  void setShowGrid(bool enabled) override;
  [[nodiscard]] bool showGrid() const override;

  void setShowDebugObjects(bool enabled) override;
  [[nodiscard]] bool showDebugObjects() const override;

  void setSkySettings(const SkyboxSettings& settings) override;
  [[nodiscard]] SkyboxSettings skySettings() const override;

  void setPostProcessSettings(const PostProcessPushConstants& settings) override;
  [[nodiscard]] PostProcessPushConstants postProcessSettings() const override;

  // ImGui mutable pointers
  [[nodiscard]] bool* showSkyboxPtr() override;
  [[nodiscard]] bool* showGridPtr() override;
  [[nodiscard]] bool* showDebugObjectsPtr() override;
  [[nodiscard]] SkyboxSettings* skySettingsPtr() override;
  [[nodiscard]] PostProcessPushConstants* postProcessPush() override;

  // Infrastructure accessors
  [[nodiscard]] RenderingStateService renderingService() override;
  [[nodiscard]] ResourceManager* resourceManager() override;
  [[nodiscard]] ModelRenderSystem* modelRenderSystem() override;

 private:
  EngineState& engineState_;
};

}  // namespace engine
