#include "Engine/Graphics/Passes/ForwardPass.hpp"

#include "Engine/EditorState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
namespace engine {
    ForwardPass::ForwardPass(ModelRenderSystem& models, GridRenderSystem& grid,
        LightSystem& light, CameraSystem& camera,
        ColliderDebugRenderSystem& collider,
        SkyboxRenderSystem&        skybox,
        Renderer& renderer, const EditorState& editor,
        std::unique_ptr<Skybox>& skyboxPtr, const SkyboxSettings& skyboxSettings)
        : RenderPassBase("Forward"), models_(models), grid_(grid), light_(light), camera_(camera), collider_(collider), skybox_(skybox), renderer_(renderer), editor_(editor), skyboxPtr_(skyboxPtr), skyboxSettings_(skyboxSettings) {}
    void ForwardPass::execute(FrameInfo& frameInfo) {
        if (editor_.debugMode != 0)
            return;
        // Draw the skybox into the offscreen color FIRST (background), so that the
        // sceneColor snapshot used by transmissive/refractive materials includes the
        // sky. If we copied before drawing the sky, glass would refract/reflect the
        // opaque scene but show black where the sky should be visible through it.
        renderer_.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
        if (skyboxPtr_) {
            skybox_.render(frameInfo, skyboxPtr_.get(), skyboxSettings_);
        } else if (skyboxSettings_.proceduralSky) {
            // Procedural sky: render with null skybox pointer
            skybox_.render(frameInfo, nullptr, skyboxSettings_);
        }
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        // Snapshot the offscreen color (opaque lighting + sky) into sceneColor for the
        // forward transmission/alpha-blend compositor to sample.
        renderer_.copyOffscreenColorToSceneColor(frameInfo.commandBuffer);
        // Re-enter the offscreen pass to draw transmissive/blended geometry and helpers.
        renderer_.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
        if (editor_.showGrid) {
            grid_.render(frameInfo);
        }
        models_.renderTransmission(frameInfo);
        models_.renderAlphaBlend(frameInfo);
        if (editor_.showDebugObjects) {
            light_.render(frameInfo);
            camera_.render(frameInfo);
        }
        if (editor_.showColliderWireframes)
            collider_.render(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        renderer_.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }
}  // namespace engine
