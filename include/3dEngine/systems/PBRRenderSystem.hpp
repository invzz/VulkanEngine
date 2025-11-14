#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../Camera.hpp"
#include "../Descriptors.hpp"
#include "../Device.hpp"
#include "../FrameInfo.hpp"
#include "../GameObject.hpp"
#include "../Model.hpp"
#include "../Pipeline.hpp"
#include "../SwapChain.hpp"

namespace engine {

  class PBRRenderSystem
  {
  public:
    PBRRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~PBRRenderSystem();

    PBRRenderSystem(const PBRRenderSystem&)            = delete;
    PBRRenderSystem& operator=(const PBRRenderSystem&) = delete;

    void render(FrameInfo& frameInfo);

    // Get or create material descriptor set for a given material
    VkDescriptorSet getMaterialDescriptorSet(const PBRMaterial& material);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createMaterialDescriptorSetLayout();
    void createMaterialDescriptorPool();
    void createDefaultTextures();

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    // Material descriptor system
    std::unique_ptr<DescriptorSetLayout> materialSetLayout;
    std::unique_ptr<DescriptorPool>      materialDescriptorPool;

    // Cache for material descriptor sets (key = material pointer address as hash)
    std::unordered_map<size_t, VkDescriptorSet> materialDescriptorCache;

    // Default textures for missing material maps
    std::shared_ptr<Texture> defaultWhiteTexture;
    std::shared_ptr<Texture> defaultNormalTexture;
  };
} // namespace engine
