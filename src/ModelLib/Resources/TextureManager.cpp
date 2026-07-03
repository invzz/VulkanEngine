#include "ModelLib/Resources/TextureManager.hpp"

#include <memory>
#include <stdexcept>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

#include "ModelLib/Resources/Texture.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

    TextureManager::TextureManager(Device& device) : device(device) {
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSet();

        placeholderTexture = Texture::createWhiteTexture(device);

        addTexture(placeholderTexture);
    }

    TextureManager::~TextureManager() = default;

    void TextureManager::createDescriptorSetLayout() {
        descriptorSetLayout = DescriptorSetLayout::Builder(device)
                                  .addBinding(0,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                      MAX_TEXTURES,
                                      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
                                  .build();
    }

    void TextureManager::createDescriptorPool() {
        descriptorPool =
            DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_TEXTURES).setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT).build();
    }

    void TextureManager::createDescriptorSet() {
        if (!descriptorPool->allocateDescriptor(descriptorSetLayout->getDescriptorSetLayout(), descriptorSet)) {
            throw std::runtime_error("failed to allocate global texture descriptor set!");
        }
    }

    uint32_t TextureManager::addTexture(const std::shared_ptr<Texture>& texture) {
        if (textureIndexMap.contains(texture.get()) != 0u) {
            return textureIndexMap[texture.get()];
        }

        if (textures.size() >= MAX_TEXTURES) {
            throw std::runtime_error("Max textures exceeded in TextureManager");
        }
        auto index = static_cast<uint32_t>(textures.size());
        textures.push_back(texture);
        textureIndexMap[texture.get()] = index;

        VkDescriptorImageInfo imageInfo = texture->getDescriptorInfo();
        updateDescriptorSet(index, imageInfo);

        return index;
    }

    void TextureManager::updateDescriptorSet(uint32_t index, VkDescriptorImageInfo& imageInfo) {
        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSet;
        write.dstBinding      = 0;
        write.dstArrayElement = index;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
    }

}  // namespace engine
