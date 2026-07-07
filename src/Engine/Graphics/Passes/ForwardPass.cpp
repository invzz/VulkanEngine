#include "Engine/Graphics/Passes/ForwardPass.hpp"

#include "Engine/EditorState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Scene/Skybox.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/ColliderDebugRenderSystem.hpp"
#include "Engine/Systems/GridRenderSystem.hpp"
#include "Engine/Systems/LightSystem.hpp"
#include "Engine/Systems/ModelRenderSystem.hpp"
#include "Engine/Systems/SelectionOutlineSystem.hpp"
#include "Engine/Systems/SkyboxRenderSystem.hpp"
namespace engine {
    ForwardPass::ForwardPass(ModelRenderSystem& models, GridRenderSystem& grid,
        LightSystem& light, CameraSystem& camera,
        ColliderDebugRenderSystem& collider,
        SelectionOutlineSystem&    outline,
        SkyboxRenderSystem&        skybox,
        Renderer& renderer, const EditorState& editor,
        std::unique_ptr<Skybox>& skyboxPtr, const SkyboxSettings& skyboxSettings)
        : RenderPassBase("Forward"), models_(models), grid_(grid), light_(light), camera_(camera), collider_(collider), outline_(outline), skybox_(skybox), renderer_(renderer), editor_(editor), skyboxPtr_(skyboxPtr), skyboxSettings_(skyboxSettings) {}
    void ForwardPass::execute(FrameInfo& frameInfo) {
        if (editor_.debugMode != 0)
            return;
        renderer_.copyOffscreenColorToSceneColor(frameInfo.commandBuffer);
        renderer_.beginOffscreenRenderPassLoadColorDepth(frameInfo.commandBuffer);
        // Render skybox first (background)
        if (skyboxPtr_) {
            skybox_.render(frameInfo, skyboxPtr_.get(), skyboxSettings_);
        } else if (skyboxSettings_.proceduralSky) {
            // Procedural sky: render with null skybox pointer
            skybox_.render(frameInfo, nullptr, skyboxSettings_);
        }
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
        outline_.render(frameInfo);
        renderer_.endOffscreenRenderPass(frameInfo.commandBuffer);
        renderer_.generateOffscreenMipmaps(frameInfo.commandBuffer);
    }
}  // namespace engine
