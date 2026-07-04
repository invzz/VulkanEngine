#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBLSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_IBLSYSTEM_HPP
#include <vulkan/vulkan.h>

#include <memory>
#include <string>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/IBL/IBLSettings.hpp"
namespace engine {
    class Pipeline;
    namespace ibl {
        class IrradianceIBL;
        class PrefilteredEnvIBL;
        class BRDFLUT;
    }  // namespace ibl
    /**
 * @brief Image-Based Lighting (IBL) System
 *
 * Generates IBL textures from an environment cubemap for PBR ambient lighting:
 * - Irradiance Map: Diffuse ambient lighting (convolved hemisphere)
 * - Prefiltered Environment Map: Specular ambient lighting (mip-mapped by
 * roughness)
 * - BRDF LUT: 2D lookup texture for split-sum approximation
 */
    class IBLSystem {
       public:
        using Settings = ibl::Settings;
        IBLSystem(Device& device);
        ~IBLSystem();
        IBLSystem(const IBLSystem&)            = delete;
        IBLSystem& operator=(const IBLSystem&) = delete;
        /**
   * @brief Generate all IBL textures from environment cubemap
   * @param skybox Source environment cubemap
   */
        void                          generateFromSkybox(Skybox& skybox);
        [[nodiscard]] bool            loadFromDisk(const std::string& directory);
        [[nodiscard]] bool            saveToDisk(const std::string& directory) const;
        void                          resetToFallback();
        void                          requestRegeneration(const Settings& settings, Skybox& skybox);
        void                          update();
        void                          updateSettings(const Settings& settings);
        [[nodiscard]] const Settings& getSettings() const {
            return settings_;
        }
        /**
   * @brief Check if IBL textures have been generated
   */
        [[nodiscard]] bool isGenerated() const {
            return generated_;
        }
        [[nodiscard]] uint64_t getGenerationCounter() const {
            return generationCounter_;
        }
        [[nodiscard]] VkDescriptorImageInfo getIrradianceDescriptorInfo() const;
        [[nodiscard]] VkDescriptorImageInfo getPrefilteredDescriptorInfo() const;
        [[nodiscard]] VkDescriptorImageInfo getBRDFLUTDescriptorInfo() const;

       private:
        void                                    cleanup();
        void                                    createFallbackResources();
        Device&                                 device_;
        bool                                    generated_         = false;
        uint64_t                                generationCounter_ = 0;
        Settings                                settings_;
        std::unique_ptr<ibl::IrradianceIBL>     irradiance_;
        std::unique_ptr<ibl::PrefilteredEnvIBL> prefiltered_;
        std::unique_ptr<ibl::BRDFLUT>           brdfLUT_;
        bool                                    regenerationRequested_ = false;
        Settings                                nextSettings_;
        Skybox*                                 nextSkybox_ = nullptr;
    };
}  // namespace engine
#endif
