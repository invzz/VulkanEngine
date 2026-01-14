#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP

#include <cstddef>
#include <memory>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {
  class ShadowSystem;
  class IBLSystem;
  struct PBRMaterial;

  // Push constants for mesh pipeline draws. Must be kept binary-compatible with GLSL
  // definition in assets/shaders/includes/mesh_push_constants.glsl
  struct MeshPushConstantData
  {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    uint32_t  meshId{0};
    uint64_t  meshletBufferAddress;
    uint64_t  meshletVerticesAddress;
    uint64_t  meshletTrianglesAddress;
    uint64_t  vertexBufferAddress;
    uint32_t  meshletOffset;
    uint32_t  meshletCount;
    glm::vec2 screenSize;
    uint32_t  cullingFlags; // Bit 0: Double Sided, Bit 1: Transparent (skip cone culling)
    uint32_t  _pad_after_culling{0u};

    // Per-instance lightmap transform (UV1 space) and texture index
    glm::vec2 lightmapUvScale{1.0f, 1.0f};
    glm::vec2 lightmapUvOffset{0.0f, 0.0f};
    uint32_t  lightmapIndex{0u};

    // Explicit padding to match GLSL std140-like push constant layout:
    // - Ensure vec2 members are aligned on 8-byte boundaries (pad after cullingFlags)
    // - Ensure total size matches the GLSL block expectations
    uint32_t _pad_end[3]{0u};
  };

  static_assert(offsetof(MeshPushConstantData, modelMatrix) == 0u, "MeshPushConstantData layout mismatch: modelMatrix offset");
  static_assert(offsetof(MeshPushConstantData, normalMatrix) == 64u, "MeshPushConstantData layout mismatch: normalMatrix offset");
  static_assert(offsetof(MeshPushConstantData, meshId) == 128u, "MeshPushConstantData layout mismatch: meshId offset");
  static_assert(offsetof(MeshPushConstantData, meshletBufferAddress) == 136u, "MeshPushConstantData layout mismatch: meshletBufferAddress offset");
  static_assert(offsetof(MeshPushConstantData, meshletVerticesAddress) == 144u, "MeshPushConstantData layout mismatch: meshletVerticesAddress offset");
  static_assert(offsetof(MeshPushConstantData, meshletTrianglesAddress) == 152u, "MeshPushConstantData layout mismatch: meshletTrianglesAddress offset");
  static_assert(offsetof(MeshPushConstantData, vertexBufferAddress) == 160u, "MeshPushConstantData layout mismatch: vertexBufferAddress offset");
  static_assert(offsetof(MeshPushConstantData, meshletOffset) == 168u, "MeshPushConstantData layout mismatch: meshletOffset offset");
  static_assert(offsetof(MeshPushConstantData, meshletCount) == 172u, "MeshPushConstantData layout mismatch: meshletCount offset");
  static_assert(offsetof(MeshPushConstantData, screenSize) == 176u, "MeshPushConstantData layout mismatch: screenSize offset");
  static_assert(offsetof(MeshPushConstantData, cullingFlags) == 184u, "MeshPushConstantData layout mismatch: cullingFlags offset");
  static_assert(offsetof(MeshPushConstantData, lightmapUvScale) == 192u, "MeshPushConstantData layout mismatch: lightmapUvScale offset");
  static_assert(offsetof(MeshPushConstantData, lightmapUvOffset) == 200u, "MeshPushConstantData layout mismatch: lightmapUvOffset offset");
  static_assert(offsetof(MeshPushConstantData, lightmapIndex) == 208u, "MeshPushConstantData layout mismatch: lightmapIndex offset");
  static_assert(sizeof(MeshPushConstantData) == 224u, "MeshPushConstantData size mismatch");

  class MaterialRenderBindings;
  class LightingRenderBindings;

  class ModelRenderSystem
  {
  public:
    ModelRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
    ~ModelRenderSystem();

    ModelRenderSystem(const ModelRenderSystem&)            = delete;
    ModelRenderSystem& operator=(const ModelRenderSystem&) = delete;

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
    void createSceneColorDescriptorResources();

    // Shared helpers for the forward compositing passes.
    void bindBaseDescriptorSets(FrameInfo& frameInfo, bool bindSceneColor) const;

    Device&                   device;
    std::unique_ptr<Pipeline> depthPrepassPipeline;
    std::unique_ptr<Pipeline> transparentPipeline;
    std::unique_ptr<Pipeline> transmissionPipeline;
    std::unique_ptr<Pipeline> standardTransparentPipeline;
    std::unique_ptr<Pipeline> standardTransmissionPipeline;
    std::unique_ptr<Pipeline> gbufferPipeline;
    VkPipelineLayout          pipelineLayout;

    std::unique_ptr<MaterialRenderBindings> materialBindings_;

    std::unique_ptr<LightingRenderBindings> lightingBindings_;

    VkDescriptorSetLayout                   sceneColorDescriptorSetLayout_{VK_NULL_HANDLE};
    std::unique_ptr<engine::DescriptorPool> sceneColorDescriptorPool_;
    std::vector<VkDescriptorSet>            sceneColorDescriptorSets_;
  };
} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
