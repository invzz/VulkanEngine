#include "Editor/ui/ModelInfoPane.hpp"

#include <imgui.h>

#include <fstream>
#include <imgui_impl_vulkan.h>
#include <sstream>

#include "Engine/Core/Logger.hpp"

#include "Editor/ui/Theme.hpp"
#include "Editor/ui/UI.hpp"
#include "IconsFontAwesome6.h"
#include "ModelLib/Resources/Model.hpp"

namespace engine::ui {

    namespace {

        std::shared_ptr<engine::Texture> loadScreenshot(engine::Device& device, const std::string& fullPath) {
            try {
                return std::make_shared<engine::Texture>(device, fullPath, /*srgb=*/true, /*flipY=*/false);
            } catch (const std::exception& e) {
                engine::Logger::warn(engine::LogChannel::Scene,
                    "[ModelInfo] failed to load screenshot ", fullPath, ": ", e.what());
                return nullptr;
            }
        }

        // Lazily read + cache the readme text for a given id.
        void ensureReadmeLoaded(ModelInfoCache& cache, const std::string& readmePath) {
            if (cache.readmeLoaded) {
                return;
            }
            cache.readmeLoaded = true;  // only attempt once per cache entry
            if (readmePath.empty()) {
                return;
            }
            std::ifstream f(readmePath);
            if (!f.is_open()) {
                return;
            }
            std::stringstream ss;
            ss << f.rdbuf();
            cache.readmeText = ss.str();
        }

    }  // namespace

    void ModelInfoPane::Draw(engine::Device& device,
        const std::string&                   id,
        const std::string&                   label,
        const std::string&                   screenshotRel,
        const std::string&                   readmeRel,
        const std::vector<std::string>& /*screenshotList*/) {
        const float paneWidth = ImGui::GetContentRegionAvail().x;
        if (paneWidth < 1.0f) {
            return;
        }

        // (Re)build cache when the active model changes.
        const std::string shotFull = screenshotRel.empty() ? "" : std::string(MODEL_PATH) + screenshotRel;
        if (cache_.id != id) {
            cache_         = ModelInfoCache{};
            cache_.id      = id;
            cache_.texture = shotFull.empty() ? nullptr : loadScreenshot(device, shotFull);
            cache_.texID   = VK_NULL_HANDLE;
            cache_.readmeText.clear();
            cache_.readmeLoaded = false;
            if (cache_.texture != nullptr) {
                cache_.texID = ImGui_ImplVulkan_AddTexture(
                    cache_.texture->getSampler(),
                    cache_.texture->getImageView(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            ensureReadmeLoaded(cache_, readmeRel.empty() ? "" : std::string(MODEL_PATH) + readmeRel);
        }

        UI::TextColored(ICON_FA_CIRCLE_INFO, Theme::Info);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", label.c_str());

        // Screenshot thumbnail (aspect-fit, capped width).
        if (cache_.texID != VK_NULL_HANDLE && cache_.texture != nullptr) {
            const float maxW   = std::min(paneWidth, 256.0f);
            const float aspect = static_cast<float>(cache_.texture->getHeight()) /
                                 static_cast<float>(cache_.texture->getWidth());
            const float drawW  = maxW;
            const float drawH  = std::max(8.0f, maxW * aspect);
            ImGui::Image(cache_.texID, ImVec2(drawW, drawH), ImVec2(0, 0), ImVec2(1, 1));
        } else if (!screenshotRel.empty()) {
            UI::TextDisabled("preview unavailable");
        }

        // Readme (scrollable).
        if (!cache_.readmeText.empty()) {
            UI::Separator();
            const ImVec2 size = ImGui::GetContentRegionAvail();
            ImGui::BeginChild("##model_readme",
                ImVec2(size.x, std::min(size.y - 8.0f, 220.0f)),
                ImGuiChildFlags_Borders,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::TextWrapped("%s", cache_.readmeText.c_str());
            ImGui::EndChild();
        } else if (!readmeRel.empty()) {
            UI::TextDisabled("no description");
        }
    }

}  // namespace engine::ui
