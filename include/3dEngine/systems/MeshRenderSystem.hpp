#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "../Device.hpp"
#include "../FrameInfo.hpp"
#include "../Pipeline.hpp"

namespace engine {
  class ShadowSystem;
  class IBLSystem;

  class MeshRenderSystem
  {
  public:
    MeshRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    ~MeshRenderSystem();

    MeshRenderSystem(const MeshRenderSystem&)            = delete;
    MeshRenderSystem& operator=(const MeshRenderSystem&) = delete;

    void render(FrameInfo& frameInfo);

    void setShadowSystem(ShadowSystem* shadowSystem);
    void setIBLSystem(IBLSystem* iblSystem);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createShadowDescriptorResources();
    void createIBLDescriptorResources();

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    VkPipelineLayout          pipelineLayout;

    ShadowSystem* currentShadowSystem_{nullptr};
    IBLSystem*    currentIBLSystem_{nullptr};

    VkDescriptorSetLayout        shadowDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             shadowDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> shadowDescriptorSets_;

    VkDescriptorSetLayout        iblDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             iblDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> iblDescriptorSets_;
  };
} // namespace engine
