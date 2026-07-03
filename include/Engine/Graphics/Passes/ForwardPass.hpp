#pragma once

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class ModelRenderSystem;
    class GridRenderSystem;
    class LightSystem;
    class CameraSystem;
    class ColliderDebugRenderSystem;
    class SelectionOutlineSystem;
    class Renderer;
    struct EditorState;

    class ForwardPass : public RenderPassBase {
       public:
        ForwardPass(ModelRenderSystem& models, GridRenderSystem& grid,
            LightSystem& light, CameraSystem& camera,
            ColliderDebugRenderSystem& collider,
            SelectionOutlineSystem&    outline,
            Renderer& renderer, const EditorState& editor);

        void execute(FrameInfo& frameInfo) override;

       private:
        ModelRenderSystem&         models_;
        GridRenderSystem&          grid_;
        LightSystem&               light_;
        CameraSystem&              camera_;
        ColliderDebugRenderSystem& collider_;
        SelectionOutlineSystem&    outline_;
        Renderer&                  renderer_;
        const EditorState&         editor_;
    };

}  // namespace engine
