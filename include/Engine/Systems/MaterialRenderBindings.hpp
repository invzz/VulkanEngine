#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_MATERIALRENDERBINDINGS_HPP
#include <memory>
#include <unordered_map>
#include <vector>

#include "Engine/Graphics/Buffer.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Device.hpp"

#include "vulkan/vulkan_core.h"
namespace engine {
    struct FrameInfo;
    struct PBRMaterial;
    struct MaterialDescriptorCacheStats {
        uint64_t cacheHits{0};
        uint64_t cacheMisses{0};
        uint64_t bufferWrites{0};
    };
    class MaterialRenderBindings {
       public:
        explicit MaterialRenderBindings(Device& device);
        ~MaterialRenderBindings();
        MaterialRenderBindings(const MaterialRenderBindings&)                        = delete;
        MaterialRenderBindings&             operator=(const MaterialRenderBindings&) = delete;
        void                                createResources();
        void                                beginFrame(int frameIndex);
        [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const {
            return descriptorSetLayout_;
        }
        [[nodiscard]] bool                         frameDescriptorSetValid(int frameIndex) const;
        void                                       enableBindCapture(bool enable);
        [[nodiscard]] std::vector<VkDescriptorSet> getCapturedBinds() const;
        [[nodiscard]] VkDescriptorSet              getFrameDescriptorSet(int frameIndex) const;
        [[nodiscard]] MaterialDescriptorCacheStats getCacheStats() const;
        void                                       resetCacheStats();
        void                                       bindMaterial(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const PBRMaterial* material, float isSelected);
        [[nodiscard]] static bool                  needsFullVariant(FrameInfo const& frameInfo, const PBRMaterial* material);

       private:
        [[nodiscard]] VkDeviceSize                          materialAtomSize() const;
        void                                                createDescriptorSetLayout();
        void                                                createBuffers();
        void                                                createPoolAndSets();
        void                                                updateDescriptorSets();
        void                                                writeAndBind(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, const void* data, VkDeviceSize dataSize);
        Device&                                             device_;
        VkDescriptorSetLayout                               descriptorSetLayout_{VK_NULL_HANDLE};
        std::unique_ptr<engine::DescriptorPool>             descriptorPool_;
        std::vector<VkDescriptorSet>                        descriptorSets_;
        std::vector<std::unique_ptr<Buffer>>                buffers_;
        std::vector<uint32_t>                               dynamicOffsetIndexByFrame_;
        std::vector<std::unordered_map<uint64_t, uint32_t>> materialOffsetCacheByFrame_;
        VkDeviceSize                                        atomSize_{0};
        mutable std::mutex                                  captureMutex_;
        bool                                                captureEnabled_ = false;
        std::vector<VkDescriptorSet>                        capturedBinds_;
        mutable std::mutex                                  allocMutex_;
        MaterialDescriptorCacheStats                        cacheStats_{};
        static constexpr uint32_t                           kMaterialSetIndex           = 4;
        static constexpr uint32_t                           kMaxMaterialRecordsPerFrame = 10000;
    };
}  // namespace engine
#endif
