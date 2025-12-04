#pragma once

#include <memory>
#include <unordered_map>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Resources/PBRMaterial.hpp"
#include "Engine/Resources/Texture.hpp"

namespace engine {

  /**
   * @brief Manages material descriptor sets and default textures
   *
   * Handles:
   * - Material descriptor set creation and caching
   * - Default fallback textures
   * - Material resource management
   */
  class MaterialSystem
  {
  public:
    MaterialSystem(Device& device);
    ~MaterialSystem() = default;

    MaterialSystem(const MaterialSystem&)            = delete;
    MaterialSystem& operator=(const MaterialSystem&) = delete;

    // Get or create material descriptor set for a given material
    VkDescriptorSet getMaterialDescriptorSet(const PBRMaterial& material);

    // Clear the descriptor cache (call when materials are modified)
    void clearDescriptorCache() { materialDescriptorCache_.clear(); }

    // Access to descriptor set layout
    VkDescriptorSetLayout getDescriptorSetLayout() const { return materialSetLayout_->getDescriptorSetLayout(); }

  private:
    void createMaterialDescriptorSetLayout();
    void createMaterialDescriptorPool();
    void createDefaultTextures();

    Device& device_;

    // Material descriptor system
    std::unique_ptr<DescriptorSetLayout> materialSetLayout_;
    std::unique_ptr<DescriptorPool>      materialDescriptorPool_;

    // Cache for material descriptor sets (key = material pointer address as hash)
    std::unordered_map<size_t, VkDescriptorSet> materialDescriptorCache_;

    // Default textures for missing material maps
    std::shared_ptr<Texture> defaultWhiteTexture_;
    std::shared_ptr<Texture> defaultNormalTexture_;
  };

} // namespace engine
