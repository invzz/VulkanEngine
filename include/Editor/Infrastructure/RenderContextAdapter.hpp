#pragma once

#include "Engine/Application/Ports/IRenderContextPort.hpp"

namespace engine {

    class RenderContext;

    // Adapter that bridges RenderContext to IRenderContextPort.
    class RenderContextAdapter final : public IRenderContextPort {
       public:
        explicit RenderContextAdapter(RenderContext* renderContext);

        LightCounts                         updateLightBuffers(int frameIndex, Scene& scene) override;
        void                                updateUBO(int frameIndex, const GlobalUbo& ubo, const GlobalUboCold& uboCold) override;
        [[nodiscard]] VkDescriptorSet       getGlobalDescriptorSet(int frameIndex) override;
        [[nodiscard]] VkDescriptorSetLayout getGlobalSetLayout() override;

       private:
        RenderContext* renderContext_ = nullptr;
    };

}  // namespace engine
