#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Systems/MaterialRenderBindings.hpp"

namespace engine {
    class ShadowSystem;
    class IBLSystem;
    struct PBRMaterial;

    // Push constants for mesh pipeline draws. Must be kept binary-compatible with GLSL
    // definition in assets/shaders/includes/mesh_push_constants.glsl
    struct MeshPushConstantData {
        glm::mat4 modelMatrix{1.0f};
        glm::mat4 normalMatrix{1.0f};
        uint32_t  meshId{0};
        uint64_t  meshletBufferAddress{0};
        uint64_t  meshletVerticesAddress{0};
        uint64_t  meshletTrianglesAddress{0};
        uint64_t  vertexBufferAddress{0};
        uint32_t  meshletOffset{0};
        uint32_t  meshletCount{0};
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
    static_assert(sizeof(MeshPushConstantData) == 176u, "MeshPushConstantData size mismatch");

    class MaterialRenderBindings;
    class LightingRenderBindings;

    class ModelRenderSystem {
       public:
        enum class VariantPolicy : std::uint8_t {
            Auto,
            ForceStandard,
            ForceFull,
        };

        ModelRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
        ~ModelRenderSystem();

        ModelRenderSystem(const ModelRenderSystem&)            = delete;
        ModelRenderSystem& operator=(const ModelRenderSystem&) = delete;

        // Reset per-frame transient state (material dynamic offsets, etc.).
        void beginFrame(int frameIndex);

        // Opt-in: enable multithreaded secondary-command-buffer recording (pilot).
        // threadCount==0 -> choose (HW threads - 1) by default.
        void enableMultiThreadedRecording(bool enable, uint32_t threadCount = 0);

        void setVariantPolicy(VariantPolicy policy) {
            variantPolicy_ = policy;
        }
        [[nodiscard]] VariantPolicy variantPolicy() const {
            return variantPolicy_;
        }

        void setShaderHotReloadEnabled(bool enabled) {
            shaderHotReloadEnabled_ = enabled;
        }
        [[nodiscard]] bool shaderHotReloadEnabled() const {
            return shaderHotReloadEnabled_;
        }
        [[nodiscard]] bool standardVariantFallbackActive() const {
            return standardVariantFallbackActive_;
        }
        [[nodiscard]] const std::string& standardVariantFallbackReason() const {
            return standardVariantFallbackReason_;
        }
        [[nodiscard]] bool isMultiThreadedRecordingEnabled() const {
            return multithreadedRecordingEnabled_;
        }

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

        [[nodiscard]] MaterialDescriptorCacheStats getMaterialDescriptorCacheStats() const;
        void                                       resetMaterialDescriptorCacheStats();

       private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout bindlessSetLayout);
        void createPipeline(VkRenderPass renderPass);
        void createSceneColorDescriptorResources();

        // Shared helpers for the forward compositing passes.
        void                    bindBaseDescriptorSets(FrameInfo& frameInfo, bool bindSceneColor) const;
        void                    hotReloadPipelinesIfNeeded();
        [[nodiscard]] Pipeline* chooseTransparentPipeline(FrameInfo const& frameInfo, const PBRMaterial* material) const;
        [[nodiscard]] Pipeline* chooseTransmissionPipeline(FrameInfo const& frameInfo, const PBRMaterial* material) const;

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

        // Stored render-pass used to record secondary command buffers' inheritance.
        VkRenderPass renderPass_ = VK_NULL_HANDLE;
        // Separate G-buffer render pass for G-buffer secondary command buffers.
        VkRenderPass gbufferRenderPass_ = VK_NULL_HANDLE;

        // Multithreading configuration (opt-in). When enabled, draw-recording is
        // partitioned and recorded to secondary command buffers on worker threads.
        bool               multithreadedRecordingEnabled_ = false;
        uint32_t           multithreadedRecordingThreads_ = 0;
        mutable std::mutex multithreadBindMutex_;  // short critical section around bindMaterial calls

        VkDescriptorSetLayout                   sceneColorDescriptorSetLayout_{VK_NULL_HANDLE};
        std::unique_ptr<engine::DescriptorPool> sceneColorDescriptorPool_;
        std::vector<VkDescriptorSet>            sceneColorDescriptorSets_;

        VariantPolicy variantPolicy_                 = VariantPolicy::Auto;
        bool          shaderHotReloadEnabled_        = true;
        bool          standardVariantFallbackActive_ = false;
        std::string   standardVariantFallbackReason_;
    };
}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MODELRENDERSYSTEM_HPP
