#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

#include "Engine/Application/Ports/IEnvironmentLightingPort.hpp"

namespace engine {

    class Device;
    class EngineState;

    // Legacy adapter that implements the expanded IEnvironmentLightingPort.
    // This class bridges EngineState to the environment lighting port interface.
    // Consider migrating to EnvironmentLightingPortAdapter for cleaner separation.
    class EnvironmentLightingAdapter final : public IEnvironmentLightingPort {
       public:
        EnvironmentLightingAdapter(Device& device, EngineState& engineState);

        // Core environment lighting sync
        void syncEnvironmentLighting(bool showSkyboxEnabled) override;

        // Skybox management
        void               loadSkybox(Device& device, const char* path) override;
        void               resetSkybox() override;
        [[nodiscard]] bool hasSkybox() const override;

        // IBL management
        bool                   loadIBLFromDisk(const char* path) override;
        void                   resetIBLToFallback() override;
        void                   updateIBL() override;
        [[nodiscard]] uint64_t getIBLGenerationCounter() const override;

        // Descriptor access for IBL
        void writeIBLDescriptors(
            const VkDescriptorImageInfo&  irradianceInfo,
            const VkDescriptorImageInfo&  prefilterInfo,
            const VkDescriptorImageInfo&  brdfInfo,
            std::vector<VkDescriptorSet>& descriptorSets,
            DescriptorSetLayout const&    layout,
            DescriptorPool const&         pool) override;

        // State access
        [[nodiscard]] bool*           showSkyboxRef() override;
        [[nodiscard]] Skybox*         getSkybox() override;
        [[nodiscard]] SkyboxSettings* getSkySettings() override;
        [[nodiscard]] ShadowSettings* getShadowSettings() override;

       private:
        Device&      device_;
        EngineState& engineState_;
        uint64_t     iblGenerationCounter_ = 0;
    };

}  // namespace engine
