#include "Editor/Infrastructure/PostProcessingAccessAdapter.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"

namespace engine {

    PostProcessingAccessAdapter::PostProcessingAccessAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    void PostProcessingAccessAdapter::recreatePostProcessingSystem(
        Device&                            device,
        VkRenderPass                       renderPass,
        std::vector<VkDescriptorSetLayout> setLayouts) {
        engineState_.setPostProcessingSystem(
            std::make_unique<PostProcessingSystem>(device, renderPass, std::move(setLayouts)));
    }

    void PostProcessingAccessAdapter::recreatePostProcessingSystemWithExistingLayout(
        Device&      device,
        VkRenderPass renderPass) {
        engineState_.setPostProcessingSystem(
            std::make_unique<PostProcessingSystem>(
                device, renderPass,
                std::vector<VkDescriptorSetLayout>{engineState_.postProcessSetLayoutRef().getDescriptorSetLayout()}));
    }

}  // namespace engine
