#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALSYSTEM_HPP
#include <memory>
#include <unordered_map>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
namespace engine {
    class PBRMaterial;
    class Texture;
    /**
 * @brief Manages material descriptor sets and default textures
 *
 * Handles:
 * - Material descriptor set creation and caching
 * - Default fallback textures
 * - Material resource management
 */
    class MaterialSystem {
       public:
        MaterialSystem(Device& device);
        ~MaterialSystem()                                = default;
        MaterialSystem(const MaterialSystem&)            = delete;
        MaterialSystem& operator=(const MaterialSystem&) = delete;
        VkDescriptorSet getMaterialDescriptorSet(const PBRMaterial& material);
        void            clearDescriptorCache() {
            materialDescriptorCache_.clear();
        }
        [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const {
            return materialSetLayout_->getDescriptorSetLayout();
        }

       private:
        void                                        createMaterialDescriptorSetLayout();
        void                                        createMaterialDescriptorPool();
        void                                        createDefaultTextures();
        Device&                                     device_;
        std::unique_ptr<DescriptorSetLayout>        materialSetLayout_;
        std::unique_ptr<DescriptorPool>             materialDescriptorPool_;
        std::unordered_map<size_t, VkDescriptorSet> materialDescriptorCache_;
        std::shared_ptr<Texture>                    defaultWhiteTexture_;
        std::shared_ptr<Texture>                    defaultNormalTexture_;
    };
}  // namespace engine
#endif
