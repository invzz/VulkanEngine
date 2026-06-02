#pragma once

#include "Engine/Application/Ports/IEnvironmentLightingPort.hpp"

namespace engine {

class EngineState;
class Device;

// Adapter that bridges EngineState to the expanded environment lighting port.
// This adapter implements all the operations that EnvironmentLightingAdapter
// currently performs directly on EngineState.
class EnvironmentLightingPortAdapter final : public IEnvironmentLightingPort {
 public:
  EnvironmentLightingPortAdapter(Device& device, EngineState& engineState);

  // Core environment lighting sync
  void syncEnvironmentLighting(bool showSkyboxEnabled) override;

  // Skybox management
  void loadSkybox(Device& device, const char* path) override;
  void resetSkybox() override;
  [[nodiscard]] bool hasSkybox() const override;

  // IBL management
  bool loadIBLFromDisk(const char* path) override;
  void resetIBLToFallback() override;
  void updateIBL() override;
  [[nodiscard]] uint64_t getIBLGenerationCounter() const override;

  // Descriptor access for IBL
  void writeIBLDescriptors(
      const VkDescriptorImageInfo& irradianceInfo,
      const VkDescriptorImageInfo& prefilterInfo,
      const VkDescriptorImageInfo& brdfInfo,
      std::vector<VkDescriptorSet>& descriptorSets,
      DescriptorSetLayout const& layout,
      DescriptorPool const& pool) override;

  // State access
  [[nodiscard]] bool* showSkyboxRef() override;
  [[nodiscard]] Skybox* getSkybox() override;
  [[nodiscard]] SkyboxSettings* getSkySettings() override;
  [[nodiscard]] ShadowSettings* getShadowSettings() override;

 private:
  Device& device_;
  EngineState& engineState_;
  uint64_t iblGenerationCounter_ = 0;
};

}  // namespace engine
