#include "Editor/ui/ViewportPanel.hpp"

#include <imgui.h>

#include "Engine/Graphics/Viewport.hpp"

namespace engine {

    ViewportPanel::ViewportPanel()
        : UIPanel(),
          extent_({400, 300}) {}

    void ViewportPanel::setViewport(Viewport* viewport, VkExtent2D extent) {
        viewport_ = viewport;
        extent_   = extent;
    }

    void ViewportPanel::render(FrameInfo& frameInfo) {
        handleResize();

        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar);

        if (viewport_) {
            ImVec2 windowSize = ImGui::GetContentRegionAvail();
            if (windowSize.x > 0 && windowSize.y > 0) {
                ImGui::Image(viewport_->getImTextureID(), windowSize, ImVec2(0, 0), ImVec2(1, 1));
            }
        }

        ImGui::End();
    }

    void ViewportPanel::handleResize() {
        ImVec2 winSize = ImGui::GetWindowSize();
        if (winSize.x > 0 && winSize.y > 0) {
            VkExtent2D newExtent = {static_cast<uint32_t>(winSize.x), static_cast<uint32_t>(winSize.y)};
            if (newExtent.width != extent_.width || newExtent.height != extent_.height) {
                extent_ = newExtent;
            }
        }
    }

}  // namespace engine
