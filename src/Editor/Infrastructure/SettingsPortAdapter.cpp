#include "Editor/Infrastructure/SettingsPortAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

SettingsPortAdapter::SettingsPortAdapter(EngineState& engineState)
    : engineState_(engineState) {}

void SettingsPortAdapter::setShowSkybox(bool enabled) {
  engineState_.showSkyboxRef() = enabled;
}

bool SettingsPortAdapter::showSkybox() const {
  return engineState_.showSkyboxRef();
}

void SettingsPortAdapter::setShowGrid(bool enabled) {
  engineState_.showGridRef() = enabled;
}

bool SettingsPortAdapter::showGrid() const {
  return engineState_.showGridRef();
}

void SettingsPortAdapter::setShowDebugObjects(bool enabled) {
  engineState_.showDebugObjectsRef() = enabled;
}

bool SettingsPortAdapter::showDebugObjects() const {
  return engineState_.showDebugObjectsRef();
}

void SettingsPortAdapter::setSkySettings(const SkyboxSettings& settings) {
  engineState_.skySettingsRef() = settings;
}

SkyboxSettings SettingsPortAdapter::skySettings() const {
  return engineState_.skySettingsRef();
}

void SettingsPortAdapter::setPostProcessSettings(const PostProcessPushConstants& settings) {
  engineState_.postProcessPushRef() = settings;
}

PostProcessPushConstants SettingsPortAdapter::postProcessSettings() const {
  return engineState_.postProcessPushRef();
}

bool* SettingsPortAdapter::showSkyboxPtr() {
  return &engineState_.showSkyboxRef();
}

bool* SettingsPortAdapter::showGridPtr() {
  return &engineState_.showGridRef();
}

bool* SettingsPortAdapter::showDebugObjectsPtr() {
  return &engineState_.showDebugObjectsRef();
}

SkyboxSettings* SettingsPortAdapter::skySettingsPtr() {
  return &engineState_.skySettingsRef();
}

PostProcessPushConstants* SettingsPortAdapter::postProcessPush() {
  return &engineState_.postProcessPushRef();
}

RenderingStateService SettingsPortAdapter::renderingService() {
  return engineState_.renderingService();
}

ResourceManager* SettingsPortAdapter::resourceManager() {
  return engineState_.resourceService().view().resourceManager;
}

ModelRenderSystem* SettingsPortAdapter::modelRenderSystem() {
  return engineState_.renderingService().view().modelRenderSystem;
}

}  // namespace engine
