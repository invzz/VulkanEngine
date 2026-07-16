#include "Editor/ui/ModelBrowser.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "Engine/Core/Logger.hpp"

#include "Editor/ui/StaticColliderRules.hpp"
#include "Editor/ui/UI.hpp"
#include "Editor/ui/UIHelpers.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"

namespace engine::ui {

    namespace {

        /// Sort keys so "glTF" precedes variants like "glTF-Binary".
        bool variantOrder(const std::string& a, const std::string& b) {
            const bool aGltf = (a == "glTF");
            const bool bGltf = (b == "glTF");
            if (aGltf != bGltf) {
                return aGltf;
            }
            return a < b;
        }

    }  // namespace

    ModelBrowser::ModelBrowser(engine::Device& device) : device_(device) {}

    void ModelBrowser::Open() {
        ImGui::OpenPopup("AddModelPopup");
    }

    void ModelBrowser::Draw(ModelInsertionOptions::StaticColliderImportMode& colliderMode,
        std::function<void(const std::string&, const std::string&, const ModelInsertionOptions&,
            ModelInsertionOptions::StaticColliderImportMode)>
            enqueueModelLoad) {
        if (!ImGui::BeginPopup("AddModelPopup")) {
            return;
        }

        UI::InputText("Filter", filterModel_, sizeof(filterModel_));

        int                colliderModeIndex = static_cast<int>(colliderMode);
        static const char* modeLabels[]      = {"Auto Detect", "Force On", "Force Off"};
        if (UI::Combo("Static Mesh Collider", &colliderModeIndex, modeLabels, 3)) {
            colliderMode = static_cast<ModelInsertionOptions::StaticColliderImportMode>(colliderModeIndex);
        }
        if (colliderMode == ModelInsertionOptions::StaticColliderImportMode::AutoDetect) {
            UI::TextDisabled(engine::StaticColliderRules::Tooltip().c_str());
        }
        UI::Separator();

        UI::InputText("Path (.gltf/.glb)", customPath_, sizeof(customPath_));
        ImGui::SameLine();
        if (UI::SmallButton("Load Path")) {
            if (customPath_[0] != '\0') {
                std::filesystem::path p(customPath_);
                if (std::filesystem::exists(p)) {
                    std::string ext = p.extension().string();
                    if (ext == ".gltf" || ext == ".glb") {
                        std::string           name = p.stem().string();
                        ModelInsertionOptions opts;
                        opts.enableTextures     = true;
                        opts.loadMaterials      = true;
                        opts.enableMorphTargets = true;
                        enqueueModelLoad(customPath_, name, opts, colliderMode);
                        customPath_[0] = '\0';
                        ImGui::CloseCurrentPopup();
                    } else {
                        UI::TextDisabled(("Unsupported format: " + std::string(ext)).c_str());
                    }
                } else {
                    UI::TextDisabled(("File not found: " + std::string(customPath_)).c_str());
                }
            }
        }

        // Split layout: left = model list (scrollable), right = info pane
        // (screenshot + readme) for the hovered/last-clicked model.
        const float popupWidth = ImGui::GetWindowWidth();
        const float listW      = std::max(220.0f, popupWidth * 0.55f);
        const float infoW      = std::max(200.0f, popupWidth - listW - 16.0f);
        ImGui::BeginChild("##model_list", ImVec2(listW, 320.0f),
            ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

        const std::string indexPath = std::string(MODEL_PATH) + "model-index.json";
        try {
            std::ifstream f(indexPath);
            if (!f.is_open()) {
                UI::TextDisabled(("Failed to open model index: " + indexPath).c_str());
            } else {
                nlohmann::json j;
                f >> j;

                for (const auto& item : j) {
                    if (!item.contains("name")) {
                        continue;
                    }
                    const std::string id    = item["name"].get<std::string>();
                    const std::string label = item.contains("display")
                                                  ? item["display"].get<std::string>()
                                                  : id;

                    if (!MatchesFilter(label, filterModel_)) {
                        continue;
                    }

                    // Screenshot / readme (relative to MODEL_PATH).
                    const std::string screenshotRel =
                        item.contains("screenshot") && item["screenshot"].is_string()
                            ? item["screenshot"].get<std::string>()
                            : "";
                    const std::string readmeRel =
                        item.contains("readme") && item["readme"].is_string()
                            ? item["readme"].get<std::string>()
                            : "";
                    std::vector<std::string> screenshotList;
                    if (item.contains("screenshots") && item["screenshots"].is_array()) {
                        for (const auto& s : item["screenshots"]) {
                            if (s.is_string()) {
                                screenshotList.push_back(s.get<std::string>());
                            }
                        }
                    }

                    // Track the active model for the info pane: update on hover or
                    // click so the pane follows the cursor and persists after it
                    // leaves.
                    if (ImGui::IsItemHovered() || ImGui::IsItemClicked()) {
                        activeModelId_    = id;
                        activeLabel_      = label;
                        activeScreenshot_ = screenshotRel;
                        activeReadme_     = readmeRel;
                    }
                    // Collect (label -> relativePath) pairs for this model.
                    std::vector<std::pair<std::string, std::string>> variants;
                    if (item.contains("variants") && item["variants"].is_object()) {
                        for (const auto& [k, v] : item["variants"].items()) {
                            if (v.is_string()) {
                                variants.emplace_back(k, v.get<std::string>());
                            }
                        }
                        std::sort(variants.begin(), variants.end(),
                            [&](const auto& a, const auto& b) { return variantOrder(a.first, b.first); });
                    }
                    // Legacy / minimal entries: derive from optional fields.
                    if (variants.empty()) {
                        if (item.contains("file") && item["file"].is_string()) {
                            variants.emplace_back("glTF", item["file"].get<std::string>());
                        } else if (item.contains("variants") && item["variants"].contains("glTF")) {
                            const std::string variantFile = item["variants"]["glTF"].get<std::string>();
                            variants.emplace_back("glTF",
                                std::string("glTF/").append(id).append("/glTF/").append(variantFile));
                        } else {
                            variants.emplace_back("glTF",
                                std::string("glTF/")
                                    .append(id)
                                    .append("/glTF/")
                                    .append(id)
                                    .append(".gltf"));
                        }
                    }

                    if (UI::TreeNode(label.c_str())) {
                        for (const auto& [variantLabel, relPath] : variants) {
                            if (UI::Selectable(variantLabel.c_str(), false,
                                    ImGuiSelectableFlags_NoAutoClosePopups)) {
                                const std::string     fullPath = std::string(MODEL_PATH).append(relPath);
                                ModelInsertionOptions opts;
                                opts.enableTextures     = true;
                                opts.loadMaterials      = true;
                                opts.enableMorphTargets = true;
                                enqueueModelLoad(fullPath, id, opts, colliderMode);
                                ImGui::CloseCurrentPopup();
                            }
                        }
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::EndChild();  // ##model_list

            // Right pane: info for the active model.
            ImGui::SameLine();
            ImGui::BeginChild("##model_info", ImVec2(infoW, 320.0f), ImGuiChildFlags_Borders);
            if (!activeModelId_.empty()) {
                infoPane_.Draw(device_, activeModelId_, activeLabel_,
                    activeScreenshot_, activeReadme_, {});
            } else {
                UI::TextDisabled("Hover a model to preview it");
            }
            ImGui::EndChild();
        } catch (const std::exception& e) {
            UI::TextDisabled(("Error parsing model index: " + std::string(e.what())).c_str());
        }
        ImGui::EndPopup();
    }

}  // namespace engine::ui
