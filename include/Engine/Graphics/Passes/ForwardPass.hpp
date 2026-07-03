#pragma once
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
    class SkyboxRenderSystem;
    class Skybox;
    struct EditorState;
    struct SkyboxSettings;

    class ForwardPass : public RenderPassBase {
       public:
        ForwardPass(ModelRenderSystem& models, GridRenderSystem& grid,
            LightSystem& light, CameraSystem& camera,
            ColliderDebugRenderSystem& collider,
            SelectionOutlineSystem&    outline,
            SkyboxRenderSystem&        skybox,
            Renderer& renderer, const EditorState& editor,
            std::unique_ptr<Skybox>& skyboxPtr, const SkyboxSettings& skyboxSettings);

        void execute(FrameInfo& frameInfo) override;

       private:
        ModelRenderSystem&         models_;
        GridRenderSystem&          grid_;
        LightSystem&               light_;
        CameraSystem&              camera_;
        ColliderDebugRenderSystem& collider_;
        SelectionOutlineSystem&    outline_;
        SkyboxRenderSystem&        skybox_;
        Renderer&                  renderer_;
        const EditorState&         editor_;
        std::unique_ptr<Skybox>&   skyboxPtr_;
        const SkyboxSettings&      skyboxSettings_;
    };

}  // namespace engine
