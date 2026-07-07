#pragma once
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
namespace engine {
    class ShadowSystem;
    class IRenderContextPort;
    class Scene;
    struct ShadowSettings;
    class ShadowPass : public RenderPassBase {
       public:
        ShadowPass(ShadowSystem& shadow, IRenderContextPort& renderCtx,
            Scene& scene, ShadowSettings& shadowSettings, float& rtShadowSoftness);
        void execute(FrameInfo& frameInfo) override;

       private:
        ShadowSystem&       shadow_;
        IRenderContextPort& renderCtx_;
        Scene&              scene_;
        ShadowSettings&     shadowSettings_;
        float&              rtShadowSoftness_;
    };
}  // namespace engine
