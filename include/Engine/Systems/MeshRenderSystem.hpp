#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {
  class ShadowSystem;
  class IBLSystem;

  struct MaterialUniformData
  {
    glm::vec4 albedo{1.0f};
    float     metallic{0.0f};
    float     roughness{0.5f};
    float     ao{1.0f};
    float     isSelected{0.0f};
    float     clearcoat{0.0f};
    float     clearcoatRoughness{0.03f};
    float     anisotropic{0.0f};
    float     anisotropicRotation{0.0f};
    float     transmission{0.0f};
    float     ior{1.5f};
    float     iridescence{0.0f};
    float     iridescenceIOR{1.3f};
    float     iridescenceThickness{100.0f};
    uint32_t  textureFlags{0};
    float     uvScale{1.0f};
    float     alphaCutoff{0.5f};
    uint32_t  alphaMode{0};
    uint32_t  albedoIndex{0};
    uint32_t  normalIndex{0};
    uint32_t  metallicIndex{0};
    uint32_t  roughnessIndex{0};
    uint32_t  aoIndex{0};
    uint32_t  emissiveIndex{0};
    uint32_t  specularGlossinessIndex{0};
    uint32_t  _pad0[2];
    glm::vec4 emissiveInfo{0.0f, 0.0f, 0.0f, 1.0f}; // rgb: color, a: strength
    glm::vec4 specularGlossinessFactor{1.0f};       // rgb: specular, a: glossiness
    uint32_t  useSpecularGlossiness{0};
    uint32_t  _padding[3];
  };

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
    void createMaterialDescriptorResources();

    Device&                   device;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<Pipeline> transparentPipeline;
    VkPipelineLayout          pipelineLayout;

    ShadowSystem* currentShadowSystem_{nullptr};
    IBLSystem*    currentIBLSystem_{nullptr};

    VkDescriptorSetLayout        shadowDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             shadowDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> shadowDescriptorSets_;

    VkDescriptorSetLayout        iblDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             iblDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> iblDescriptorSets_;

    VkDescriptorSetLayout                materialDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool                     materialDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet>         materialDescriptorSets_;
    std::vector<std::unique_ptr<Buffer>> materialBuffers_;
  };
} // namespace engine
