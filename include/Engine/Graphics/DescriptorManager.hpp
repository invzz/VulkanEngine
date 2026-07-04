#ifndef ENGINE_DESCRIPTORMANAGER_HPP
#define ENGINE_DESCRIPTORMANAGER_HPP
#include <memory>
#include <vector>

#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Renderer.hpp"
namespace engine {
    /**
 * @brief Centralized descriptor pool/layout/set management.
 *
 * Replaces the scattered descriptor state in EngineState. Owns all descriptor
 * pools, set layouts, and per-frame descriptor sets for G-buffer, deferred IBL,
 * deferred shadow, and post-processing.
 */
    class DescriptorManager {
       public:
        DescriptorManager()                                    = default;
        ~DescriptorManager()                                   = default;
        DescriptorManager(const DescriptorManager&)            = delete;
        DescriptorManager& operator=(const DescriptorManager&) = delete;
        /**
     * @brief Create descriptor pools and set layouts for all descriptor domains.
     */
        void createDescriptorResources(Device& device, Renderer& renderer);
        /**
     * @brief Allocate per-frame descriptor sets for all domains.
     */
        void                          allocatePerFrameDescriptors(Renderer& renderer);
        [[nodiscard]] DescriptorPool& gbufferPool() const {
            return *gbufferPool_;
        }
        [[nodiscard]] DescriptorSetLayout& gbufferSetLayout() const {
            return *gbufferSetLayout_;
        }
        [[nodiscard]] VkDescriptorSet  gbufferDescriptorSet(int frameIndex) const;
        [[nodiscard]] VkDescriptorSet& gbufferDescriptorSetRef(int frameIndex);
        [[nodiscard]] DescriptorPool&  deferredIblPool() const {
            return *deferredIblPool_;
        }
        [[nodiscard]] DescriptorSetLayout& deferredIblSetLayout() const {
            return *deferredIblSetLayout_;
        }
        [[nodiscard]] VkDescriptorSet               deferredIblDescriptorSet(int frameIndex) const;
        [[nodiscard]] std::vector<VkDescriptorSet>& deferredIblDescriptorSets();
        [[nodiscard]] DescriptorPool&               deferredShadowPool() const {
            return *deferredShadowPool_;
        }
        [[nodiscard]] DescriptorSetLayout& deferredShadowSetLayout() const {
            return *deferredShadowSetLayout_;
        }
        [[nodiscard]] VkDescriptorSet  deferredShadowDescriptorSet(int frameIndex) const;
        [[nodiscard]] VkDescriptorSet& deferredShadowDescriptorSetRef(int frameIndex);
        [[nodiscard]] DescriptorPool&  postProcessPool() const {
            return *postProcessPool_;
        }
        [[nodiscard]] DescriptorSetLayout& postProcessSetLayout() const {
            return *postProcessSetLayout_;
        }
        [[nodiscard]] VkDescriptorSet  postProcessDescriptorSet(int frameIndex) const;
        [[nodiscard]] VkDescriptorSet& postProcessDescriptorSetRef(int frameIndex);
        /**
     * @brief Update G-buffer descriptors for the given frame.
     * @param frameIndex Current frame index
     * @param renderer Renderer for image info queries
     */
        void updateGbufferDescriptors(int frameIndex, Renderer& renderer);
        /**
     * @brief Update shadow descriptors for the given frame.
     * @param frameIndex Current frame index
     * @param shadowSystem Shadow system providing shadow map image infos
     * @param device Vulkan device for vkUpdateDescriptorSets
     */
        void updateShadowDescriptors(int frameIndex, class ShadowSystem& shadowSystem, Device& device);
        /**
                 * @brief Update post-processing descriptors with the current offscreen
                 * color and depth image views. Must be called after offscreen framebuffer
                 * resize so the descriptor set points to the new (valid) images.
                 */
        void updatePostProcessDescriptors(int frameIndex, Renderer& renderer);
        /**
                 * @brief Recreate post-processing descriptor sets with an existing layout.
                 */
        void recreatePostProcessDescriptorSets(Device& device, Renderer& renderer, VkDescriptorSetLayout existingLayout);

       private:
        std::unique_ptr<DescriptorPool>      gbufferPool_;
        std::unique_ptr<DescriptorSetLayout> gbufferSetLayout_;
        std::vector<VkDescriptorSet>         gbufferDescriptorSets_;
        std::unique_ptr<DescriptorPool>      deferredIblPool_;
        std::unique_ptr<DescriptorSetLayout> deferredIblSetLayout_;
        std::vector<VkDescriptorSet>         deferredIblDescriptorSets_;
        std::unique_ptr<DescriptorPool>      deferredShadowPool_;
        std::unique_ptr<DescriptorSetLayout> deferredShadowSetLayout_;
        std::vector<VkDescriptorSet>         deferredShadowDescriptorSets_;
        std::unique_ptr<DescriptorPool>      postProcessPool_;
        std::unique_ptr<DescriptorSetLayout> postProcessSetLayout_;
        std::vector<VkDescriptorSet>         postProcessDescriptorSets_;
    };
}  // namespace engine
#endif
