#pragma once
#include <algorithm>
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

  // Forward declarations
  class MaterialSystem;
  class ShadowMap;
  class ShadowSystem;
  class IBLSystem;

  class PBRRenderSystem
  {
  public:
    static constexpr int MAX_SHADOW_MAPS      = 4;
    static constexpr int MAX_CUBE_SHADOW_MAPS = 4;

    PBRRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~PBRRenderSystem();

    PBRRenderSystem(const PBRRenderSystem&)            = delete;
    PBRRenderSystem& operator=(const PBRRenderSystem&) = delete;

    void render(FrameInfo& frameInfo);

    // Clear the material descriptor cache (call when materials are modified)
    void clearDescriptorCache();

    // Set shadow system for rendering (call before render each frame)
    void setShadowSystem(ShadowSystem* shadowSystem);

    // Set IBL system for rendering (call before render each frame)
    void setIBLSystem(IBLSystem* iblSystem);

    // Legacy: Set single shadow map (deprecated, use setShadowSystem)
    void setShadowMap(ShadowMap* shadowMap);

    // Get shadow descriptor set layout for pipeline creation
    VkDescriptorSetLayout getShadowDescriptorSetLayout() const { return shadowDescriptorSetLayout_; }

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createShadowDescriptorResources();
    void createIBLDescriptorResources();

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    // Material management subsystem
    std::unique_ptr<MaterialSystem> materialSystem;

    // Shadow map resources
    VkDescriptorSetLayout        shadowDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool             shadowDescriptorPool_      = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> shadowDescriptorSets_;
    ShadowMap*                   currentShadowMap_    = nullptr;
    ShadowSystem*                currentShadowSystem_ = nullptr;

    // IBL resources
    VkDescriptorSetLayout        iblDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool             iblDescriptorPool_      = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> iblDescriptorSets_;
    IBLSystem*                   currentIBLSystem_ = nullptr;
  };
} // namespace engine
