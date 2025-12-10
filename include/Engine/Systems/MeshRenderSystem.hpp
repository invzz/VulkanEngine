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
    glm::vec4 emissiveInfo{0.0f, 0.0f, 0.0f, 1.0f}; // rgb: color, a: strength
    glm::vec4 specularGlossinessFactor{1.0f};       // rgb: specular, a: glossiness

    // Packed float parameters
    // Col 0: metallic, roughness, ao, isSelected
    // Col 1: clearcoat, clearcoatRoughness, anisotropic, anisotropicRotation
    // Col 2: transmission, ior, iridescence, iridescenceIOR
    // Col 3: iridescenceThickness, uvScale, alphaCutoff, padding
    glm::mat4 params{0.0f};

    // Packed uint parameters
    // x: textureFlags, y: alphaMode, z: albedoIndex, w: normalIndex
    glm::uvec4 flagsAndIndices0{0};
    // x: metallicIndex, y: roughnessIndex, z: aoIndex, w: emissiveIndex
    glm::uvec4 indices1{0};
    // x: specularGlossinessIndex, y: useSpecularGlossiness, z: pad, w: pad
    glm::uvec4 indices2{0};
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
