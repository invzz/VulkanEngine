#include "Editor/ui/ViewportPanel.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include "Engine/Graphics/ImGuiManager.hpp"
#include "Engine/Graphics/ViewportTexture.hpp"

namespace engine {

    ViewportPanel::ViewportPanel()
        : UIPanel(),
          extent_({400, 300}) {
    }

    void ViewportPanel::initialize(ImGuiManager& imguiManager, ViewportTexture* viewportTexture, VkExtent2D extent) {
        imguiManager_    = &imguiManager;
        viewportTexture_ = viewportTexture;
        extent_          = extent;

        // Register the viewport texture with the ImGui Vulkan backend.
        // ImTextureID must be a pointer to a registered texture descriptor set,
        // not a raw VkImageView pointer.
        textureHandle_ = reinterpret_cast<ImTextureID>(
            ImGui_ImplVulkan_AddTexture(
                viewportTexture_->getSampler(),
                viewportTexture_->getImageView(),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    void ViewportPanel::render(FrameInfo& frameInfo) {
        // Handle resize
        handleResize();

        // Display viewport texture via ImGui
        // (The copy from offscreen color to viewport texture is handled by the render graph's ViewportCopy pass)
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

        ImVec2 windowSize = ImGui::GetContentRegionAvail();
        if (windowSize.x > 0 && windowSize.y > 0) {
            ImGui::Image(textureHandle_, windowSize, ImVec2(0, 0), ImVec2(1, 1));
        }

        ImGui::End();
    }

    void ViewportPanel::renderViewportContent(FrameInfo& frameInfo) {
        // Transition viewport texture to color attachment
        viewportTexture_->transitionToColorAttachment(frameInfo.commandBuffer);

        // Render the scene to the viewport texture.
        // This is wired into the render graph as a separate pass.
        // For now, the copy from offscreen color happens in the render graph.

        // Transition back to shader-read-only
        viewportTexture_->transitionToShaderReadOnly(frameInfo.commandBuffer);
    }

    void ViewportPanel::handleResize() {
        // Get current ImGui window size (we're rendering inside the Viewport window)
        ImVec2 winSize = ImGui::GetWindowSize();
        if (winSize.x > 0 && winSize.y > 0) {
            VkExtent2D newExtent = {static_cast<uint32_t>(winSize.x), static_cast<uint32_t>(winSize.y)};

            if (newExtent.width != extent_.width || newExtent.height != extent_.height) {
                extent_ = newExtent;
            }
        }
    }

}  // namespace engine
