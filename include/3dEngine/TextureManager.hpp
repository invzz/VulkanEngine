#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "3dEngine/Descriptors.hpp"
#include "3dEngine/Device.hpp"
#include "3dEngine/Texture.hpp"

namespace engine {

  class TextureManager
  {
  public:
    static constexpr uint32_t MAX_TEXTURES = 1024;

    TextureManager(Device& device);
    ~TextureManager();

    TextureManager(const TextureManager&)            = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Returns the global index of the texture
    uint32_t addTexture(std::shared_ptr<Texture> texture);

    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout->getDescriptorSetLayout(); }
    VkDescriptorSet       getDescriptorSet() const { return descriptorSet; }

  private:
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void updateDescriptorSet(uint32_t index, VkDescriptorImageInfo& imageInfo);

    Device& device;

    std::unique_ptr<DescriptorSetLayout> descriptorSetLayout;
    std::unique_ptr<DescriptorPool>      descriptorPool;
    VkDescriptorSet                      descriptorSet;

    std::vector<std::shared_ptr<Texture>>  textures;
    std::unordered_map<Texture*, uint32_t> textureIndexMap;

    // Placeholder texture for empty slots
    std::shared_ptr<Texture> placeholderTexture;
  };

} // namespace engine
