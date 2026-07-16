#include "Engine/Graphics/RenderPipeline.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"
#include "Engine/Graphics/Passes/CompositionPass.hpp"
#include "Engine/Graphics/Passes/ComputePass.hpp"
#include "Engine/Graphics/Passes/DeferredLightingPass.hpp"
#include "Engine/Graphics/Passes/DepthPrepass.hpp"
#include "Engine/Graphics/Passes/ForwardPass.hpp"
#include "Engine/Graphics/Passes/GbufferPass.hpp"
#include "Engine/Graphics/Passes/PostProcessPass.hpp"
#include "Engine/Graphics/Passes/ShadowPass.hpp"
#include "Engine/Graphics/Passes/UpdatePass.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace engine {
    RenderPipeline::RenderPipeline(Renderer& r) : renderer(r) {}
    void RenderPipeline::setRenderGraph(std::unique_ptr<RenderGraph> graph) {
        renderGraph = std::move(graph);
    }
    void RenderPipeline::buildDefaultGraph(EngineState& engineState, Renderer& renderer, Device& device, float rtShadowSoftness, CompositionPass::UIRenderFn renderUi, Window& window) {
        auto graph = std::make_unique<RenderGraph>();
        graph->addPass(std::make_unique<UpdatePass>(
            engineState.systemPtr<ObjectSelectionSystem>(),
            engineState.systemPtr<InputSystem>(),
            engineState.systemPtr<JoltPhysicsSystem>(),
            engineState.physicsRunning(),
            renderer));
        graph->addPass(std::make_unique<ComputePass>(engineState.systemPtr<AnimationSystem>()));
        graph->addPass(std::make_unique<ShadowPass>(
            engineState.system<ShadowSystem>(),
            engineState.renderContext(),
            engineState.scene(),
            engineState.shadowSettings(),
            rtShadowSoftness));
        graph->addPass(std::make_unique<DepthPrepass>(
            engineState.system<ModelRenderSystem>(),
            renderer));
        graph->addPass(std::make_unique<GbufferPass>(
            engineState.system<ModelRenderSystem>(),
            engineState, renderer,
            engineState.renderContext()));
        graph->addPass(std::make_unique<DeferredLightingPass>(
            engineState.system<DeferredLightingSystem>(),
            engineState.system<ShadowSystem>(),
            engineState, renderer, device,
            engineState.renderContext()));
        graph->addPass(std::make_unique<ForwardPass>(
            engineState.system<ModelRenderSystem>(),
            engineState.system<GridRenderSystem>(),
            engineState.system<LightSystem>(),
            engineState.system<CameraSystem>(),
            engineState.system<ColliderDebugRenderSystem>(),
            *engineState.systemPtr<SkyboxRenderSystem>(),
            renderer,
            engineState.editor(),
            engineState.skybox(),
            engineState.skySettings()));
        // Selection mask: render selected geometry (depth disabled) into a 1-channel
        // mask so the full silhouette is captured. Runs after the scene, before the
        // screen-space composite that turns it into the Blender-style rim.
        graph->addPass(std::make_unique<LambdaRenderPass>("SelectionMask",
            [&engineState, &renderer](FrameInfo& frameInfo) {
                renderer.beginSelectionMaskRenderPass(frameInfo.commandBuffer);
                engineState.system<SelectionMaskSystem>().render(frameInfo);
                renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<LambdaRenderPass>("TransitionToReadOnly",
            [&renderer](FrameInfo& frameInfo) {
                renderer.transitionColorToShaderReadOnly(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<PostProcessPass>(renderer, engineState));
        // Selection composite: full-screen edge-detect on the mask, blended on top of
        // the tonemapped post-fx image. This is the topmost scene layer (above all
        // geometry and post-fx) before the ImGui viewport overlay.
        graph->addPass(std::make_unique<LambdaRenderPass>("SelectionComposite",
            [&engineState, &renderer](FrameInfo& frameInfo) {
                renderer.beginSelectionOutlineRenderPass(frameInfo.commandBuffer);
                engineState.system<SelectionCompositeSystem>().render(frameInfo);
                renderer.endOffscreenRenderPass(frameInfo.commandBuffer);
            }));
        graph->addPass(std::make_unique<CompositionPass>(renderer,
            renderUi,
            window));
        setRenderGraph(std::move(graph));
    }
    void RenderPipeline::execute(FrameInfo& frameInfo) {
        if (renderGraph) {
            renderGraph->execute(frameInfo);
        }
    }
}  // namespace engine
