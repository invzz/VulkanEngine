#include "Editor/Infrastructure/DescriptorAccessAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

    DescriptorAccessAdapter::DescriptorAccessAdapter(EngineState& engineState)
        : engineState_(engineState) {}

    DescriptorPool& DescriptorAccessAdapter::getDescriptorPool() {
        return engineState_.postProcessPoolRef();
    }

    DescriptorSetLayout& DescriptorAccessAdapter::getPostProcessSetLayout() {
        return engineState_.postProcessSetLayoutRef();
    }

    VkDescriptorSet DescriptorAccessAdapter::getGbufferDescriptorSet(uint32_t frameIndex) {
        return engineState_.getGbufferDescriptorSet(frameIndex);
    }

    VkDescriptorSet DescriptorAccessAdapter::getDeferredShadowDescriptorSet(uint32_t frameIndex) {
        return engineState_.getDeferredShadowDescriptorSet(frameIndex);
    }

    VkDescriptorSet DescriptorAccessAdapter::getDeferredIblDescriptorSet(uint32_t frameIndex) {
        return engineState_.getDeferredIblDescriptorSet(frameIndex);
    }

    VkDescriptorSet DescriptorAccessAdapter::getPostProcessDescriptorSet(uint32_t frameIndex) {
        return engineState_.getPostProcessDescriptorSet(frameIndex);
    }

    VkDescriptorSet& DescriptorAccessAdapter::postProcessDescriptorSetRef(int frameIndex) {
        return engineState_.postProcessDescriptorSetRef(frameIndex);
    }

    DescriptorPool& DescriptorAccessAdapter::gbufferPoolRef() {
        return engineState_.gbufferPoolRef();
    }

    DescriptorSetLayout& DescriptorAccessAdapter::gbufferSetLayoutRef() {
        return engineState_.gbufferSetLayoutRef();
    }

    VkDescriptorSet& DescriptorAccessAdapter::gbufferDescriptorSetRef(int frameIndex) {
        return engineState_.gbufferDescriptorSetRef(frameIndex);
    }

    VkDescriptorSet& DescriptorAccessAdapter::deferredShadowDescriptorSetRef(int frameIndex) {
        return engineState_.deferredShadowDescriptorSetRef(frameIndex);
    }

}  // namespace engine
