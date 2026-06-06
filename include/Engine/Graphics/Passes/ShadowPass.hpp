#pragma once

#include "Engine/Application/Ports/IRenderContextPort.hpp"
#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Application/StateViews/SceneRuntimeStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ShadowPass : public IRenderPass {
       public:
        ShadowPass(RenderingStateView rendering, SceneRuntimeStateView sceneRuntime, IRenderContextPort* renderContextPort)
            : rendering_(rendering), sceneRuntime_(sceneRuntime), renderContextPort_(renderContextPort) {}

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override {
            static std::string name = "Shadow";
            return name;
        }

       private:
        RenderingStateView    rendering_;
        SceneRuntimeStateView sceneRuntime_;
        IRenderContextPort*   renderContextPort_ = nullptr;
    };

}  // namespace engine
