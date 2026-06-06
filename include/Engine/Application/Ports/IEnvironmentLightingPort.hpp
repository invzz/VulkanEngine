#pragma once

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

namespace engine {

    class Device;
    class IBLSystem;
    class Skybox;
    class SkyboxSettings;
    class ShadowSettings;
    class DescriptorPool;
    class DescriptorSetLayout;

    // Expanded port for environment lighting operations.
    // This captures all the operations that EnvironmentLightingAdapter performs
    // so that the adapter can be refactored to depend only on ports.
    class IEnvironmentLightingPort {
       public:
        virtual ~IEnvironmentLightingPort() = default;

        // Core environment lighting sync
        virtual void syncEnvironmentLighting(bool showSkyboxEnabled) = 0;

        // Skybox management
        virtual void               loadSkybox(Device& device, const char* path) = 0;
        virtual void               resetSkybox()                                = 0;
        [[nodiscard]] virtual bool hasSkybox() const                            = 0;

        // IBL management
        virtual bool                   loadIBLFromDisk(const char* path) = 0;
        virtual void                   resetIBLToFallback()              = 0;
        virtual void                   updateIBL()                       = 0;
        [[nodiscard]] virtual uint64_t getIBLGenerationCounter() const   = 0;

        // Descriptor access for IBL
        virtual void writeIBLDescriptors(
            const VkDescriptorImageInfo&  irradianceInfo,
            const VkDescriptorImageInfo&  prefilterInfo,
            const VkDescriptorImageInfo&  brdfInfo,
            std::vector<VkDescriptorSet>& descriptorSets,
            DescriptorSetLayout const&    layout,
            DescriptorPool const&         pool) = 0;

        // State access
        [[nodiscard]] virtual bool*           showSkyboxRef()     = 0;
        [[nodiscard]] virtual Skybox*         getSkybox()         = 0;
        [[nodiscard]] virtual SkyboxSettings* getSkySettings()    = 0;
        [[nodiscard]] virtual ShadowSettings* getShadowSettings() = 0;
    };

}  // namespace engine
