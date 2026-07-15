#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONMASKSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONMASKSYSTEM_HPP
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Pipeline.hpp"
#include "Engine/Scene/components/SubMeshComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"

#include <memory>
namespace engine {
    /**
 * @brief Renders the selected model (or a single selected sub-mesh) into a
 *        single-channel selection mask, depth test disabled, so the full
 *        projected silhouette is captured. SelectionCompositeSystem turns this
 *        mask into a Blender-style orange rim drawn on top of the final image.
 */
    class SelectionMaskSystem {
       public:
        SelectionMaskSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~SelectionMaskSystem();
        SelectionMaskSystem(const SelectionMaskSystem&)            = delete;
        SelectionMaskSystem& operator=(const SelectionMaskSystem&) = delete;

        void render(FrameInfo& frameInfo) const;

       private:
        Device&                                      device_;
        std::unique_ptr<Pipeline>                   pipeline_;
        VkPipelineLayout                             pipelineLayout_{VK_NULL_HANDLE};
        VkDescriptorSetLayout                       globalSetLayout_{VK_NULL_HANDLE};

        /// Resolve the model entity that owns the selection (handles node
        /// selection, which maps up to the model that contains the node).
        static entt::entity resolveModelEntity(const Scene& scene, entt::entity selected);
    };
}  // namespace engine
#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONMASKSYSTEM_HPP
