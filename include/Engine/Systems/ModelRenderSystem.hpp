#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP

#include <memory>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {
  class ShadowSystem;
  class IBLSystem;
  struct PBRMaterial;

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

    VkDescriptorSetLayout        sceneColorDescriptorSetLayout_{VK_NULL_HANDLE};
    VkDescriptorPool             sceneColorDescriptorPool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> sceneColorDescriptorSets_;
  };
} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
