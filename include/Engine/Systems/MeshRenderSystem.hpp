#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MESHRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MESHRENDERSYSTEM_HPP

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
  struct PBRMaterial;

  struct MaterialUniformData
  {
    glm::vec4 albedo{1.0f};
    glm::vec4 emissiveInfo{0.0f, 0.0f, 0.0f, 1.0f};            // rgb: color, a: strength
    glm::vec4 specularGlossinessFactor{1.0f};                  // rgb: specular, a: glossiness
    glm::vec4 attenuationColorAndDist{1.0f, 1.0f, 1.0f, 1.0f}; // rgb: color, a: distance

    // Packed float parameters
    // Col 0: metallic, roughness, ao, isSelected
    // Col 1: clearcoat, clearcoatRoughness, anisotropic, anisotropicRotation
    // Col 2: transmission, ior, iridescence, iridescenceIOR
    // Col 3: iridescenceThickness, uvScale, alphaCutoff, thickness
    glm::mat4 params{0.0f};

    // Packed uint parameters
    // x: textureFlags, y: alphaMode, z: albedoIndex, w: normalIndex
    glm::uvec4 flagsAndIndices0{0};
    // x: metallicIndex, y: roughnessIndex, z: aoIndex, w: emissiveIndex
    glm::uvec4 indices1{0};
    // x: specularGlossinessIndex, y: useSpecularGlossiness, z: transmissionIndex, w: clearcoatIndex
    glm::uvec4 indices2{0};
    // x: clearcoatRoughnessIndex, y: clearcoatNormalIndex, z: pad, w: pad
    glm::uvec4 indices3{0};
  };

  class MeshRenderSystem
  {
  public:
    MeshRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    ~MeshRenderSystem();

    MeshRenderSystem(const MeshRenderSystem&)            = delete;
    MeshRenderSystem& operator=(const MeshRenderSystem&) = delete;

    // Reset per-frame transient state (material dynamic offsets, etc.).
    void beginFrame(int frameIndex);

    // Multi-pass rendering entry points.
    void renderGbuffer(FrameInfo& frameInfo);
    void renderTransmission(FrameInfo& frameInfo);
    void renderAlphaBlend(FrameInfo& frameInfo);

    // Must be called once after the G-buffer render pass exists.
    void createGbufferPipeline(VkRenderPass renderPass);

    // Update the scene-color copy descriptor for screen-space refraction.
    void updateSceneColorDescriptor(int frameIndex, VkDescriptorImageInfo const& sceneColorInfo);

    void renderDepthPrepass(FrameInfo& frameInfo);

    // Must be called once after the offscreen depth-prepass render pass exists.
    void createDepthPrepassPipeline(VkRenderPass renderPass);

    void setShadowSystem(ShadowSystem* shadowSystem);
    void setIBLSystem(IBLSystem* iblSystem);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    void createPipeline(VkRenderPass renderPass);
    void createShadowDescriptorResources();
    void createIBLDescriptorResources();
    void createMaterialDescriptorResources();
    void createSceneColorDescriptorResources();

    // Shared helpers for the forward compositing passes.
    [[nodiscard]] VkDeviceSize materialAtomSize() const;
    void                       bindBaseDescriptorSets(FrameInfo& frameInfo, bool bindSceneColor) const;
    void                       bindShadowDescriptorSet(FrameInfo& frameInfo);
    void                       bindIBLDescriptorSet(FrameInfo& frameInfo);
    bool                       materialNeedsFullVariant(FrameInfo const& frameInfo, const PBRMaterial* mat) const;
    MaterialUniformData        buildMaterialUniformData(const PBRMaterial* pMaterial, float isSelected) const;
    void                       buildWriteAndBindMaterial(FrameInfo& frameInfo, char* mappedData, VkDeviceSize atomSize, uint32_t& dynamicOffsetIndex, const PBRMaterial* pMaterial, float isSelected);
    void                       writeAndBindMaterial(FrameInfo& frameInfo, char* mappedData, VkDeviceSize atomSize, uint32_t& dynamicOffsetIndex, MaterialUniformData const& matData);

    Device&                   device;
    std::unique_ptr<Pipeline> depthPrepassPipeline;
    std::unique_ptr<Pipeline> transparentPipeline;
    std::unique_ptr<Pipeline> transmissionPipeline;
    std::unique_ptr<Pipeline> standardTransparentPipeline;
    std::unique_ptr<Pipeline> standardTransmissionPipeline;
    std::unique_ptr<Pipeline> gbufferPipeline;
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

    VkDescriptorSetLayout        sceneColorDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             sceneColorDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> sceneColorDescriptorSets_;

    std::vector<uint32_t> dynamicOffsetIndexByFrame_;
  };
} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MESHRENDERSYSTEM_HPP
