#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONCOMPOSITESYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONCOMPOSITESYSTEM_HPP
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Pipeline.hpp"

#include <memory>
#include <vector>
namespace engine {
    class Renderer;
    /**
 * @brief Full-screen composite that turns the selection mask into a Blender-style
 *        orange rim, drawn ON TOP of the tonemapped post-fx image. Runs after
 *        post-processing and before the viewport displays the result, so the
 *        outline sits above every other layer (it is not tonemapped or depth-tested).
 */
    class SelectionCompositeSystem {
       public:
        SelectionCompositeSystem(Device& device, VkRenderPass renderPass, Renderer& renderer);
        ~SelectionCompositeSystem();
        SelectionCompositeSystem(const SelectionCompositeSystem&)            = delete;
        SelectionCompositeSystem& operator=(const SelectionCompositeSystem&) = delete;

        void render(FrameInfo& frameInfo) const;

       private:
        Device&                                      device_;
        Renderer&                                    renderer_;
        std::unique_ptr<Pipeline>                   pipeline_;
        VkPipelineLayout                             pipelineLayout_{VK_NULL_HANDLE};
        std::unique_ptr<DescriptorSetLayout>        descriptorSetLayout_;
        std::unique_ptr<DescriptorPool>             descriptorPool_;
        std::vector<VkDescriptorSet>                descriptorSets_;
    };
}  // namespace engine
#endif  // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_SELECTIONCOMPOSITESYSTEM_HPP
