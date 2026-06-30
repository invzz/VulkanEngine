#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ShadowSystem;
    class IRenderContextPort;
    class Scene;
    struct ShadowSettings;

    class ShadowPass : public IRenderPass {
       public:
        ShadowPass(ShadowSystem& shadow, IRenderContextPort& renderCtx,
            Scene& scene, ShadowSettings& shadowSettings);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override;

       private:
        ShadowSystem&       shadow_;
        IRenderContextPort& renderCtx_;
        Scene&              scene_;
        ShadowSettings&     shadowSettings_;
    };

}  // namespace engine
