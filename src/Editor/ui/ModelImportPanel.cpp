#include "Editor/ui/ModelImportPanel.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/SceneUtils.hpp"
#include "ModelLib/Resources/Model.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "nlohmann/json_fwd.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

ModelImportPanel::ModelImportPanel(Device& device, EngineState* engineState)
    : device_(device), engineState_(engineState) {
  loadModelIndex();
}

void ModelImportPanel::render(FrameInfo& /*frameInfo*/) {
  if (!visible_) return;

  if (ImGui::Begin("Assets", &visible_)) {
    if (ImGui::CollapsingHeader("Meshlet Settings")) {
      ImGui::TextWrapped("Configure meshlet generation for GPU-driven rendering. Smaller meshlets improve culling but increase count.");
      ImGui::Separator();

      ImGui::SliderInt("Max Vertices", &meshletMaxVertices_, 8, 64, "%d");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum unique vertices per meshlet (8-64). Lower = more meshlets, better culling.");
      }

      ImGui::SliderInt("Max Triangles", &meshletMaxTriangles_, 8, 124, "%d");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Maximum triangles per meshlet (8-124). Lower = more meshlets, better culling.");
      }

      ImGui::SliderFloat("Cone Weight", &meshletConeWeight_, 0.0f, 1.0f, "%.2f");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("0 = favor spatial locality, 1 = favor backface culling effectiveness.");
      }

      ImGui::SliderFloat("Max Radius (m)", &meshletMaxRadius_, 0.0f, 10.0f, "%.1f");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Maximum bounding sphere radius in meters. 0 = disabled.\n"
            "Meshlets larger than this will be split for better culling.\n"
            "Recommended: 2-4m for indoor scenes (Sponza), 1-2m for detailed objects.");
      }

      if (ImGui::Button("Reset to Defaults")) {
        meshletMaxVertices_ = 64;
        meshletMaxTriangles_ = 124;
        meshletConeWeight_ = 0.0f;
        meshletMaxRadius_ = 0.0f;
      }
      ImGui::SameLine();
      if (ImGui::Button("High Quality Culling")) {
        meshletMaxVertices_ = 32;
        meshletMaxTriangles_ = 64;
        meshletConeWeight_ = 0.5f;
        meshletMaxRadius_ = 2.0f;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Preset for indoor scenes with large walls/floors (e.g., Sponza).");
      }
    }

    if (ImGui::CollapsingHeader("Import Model", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::InputText("glTF Path", modelPath_, sizeof(modelPath_));
      ImGui::SameLine();
      if (ImGui::Button("Browse")) {
        std::string const defaultPath = std::string(MODEL_PATH) + "/glTF/";
        std::snprintf(modelPath_, sizeof(modelPath_), "%s", defaultPath.c_str());
      }

      if (ImGui::Button("Load Model")) {
        std::string fullPath = modelPath_;
        if (fullPath[0] != '/') {
          fullPath = std::string(MODEL_PATH) + "/" + fullPath;
        }
        loadModel(fullPath);
      }
    }

    if (ImGui::CollapsingHeader("Available Models", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginChild("AvailableModelsScroll", ImVec2(0, 0), 1);

      float const windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
      ImGuiStyle const& style = ImGui::GetStyle();
      float const buttonSize = 128.0f;
      float const spacing = style.ItemSpacing.x;

      for (size_t i = 0; i < availableModels_.size(); i++) {
        const auto& entry = availableModels_[i];

        assert(i <= static_cast<size_t>(std::numeric_limits<int>::max()));
        ImGui::PushID(static_cast<int>(i));

        if (entry.descriptorSet != VK_NULL_HANDLE) {
          if (ImGui::ImageButton("##image", (ImTextureID)entry.descriptorSet, ImVec2(buttonSize, buttonSize))) {
            std::string const fullPath = std::string(MODEL_PATH) + "/" + entry.relativePath;
            loadModel(fullPath, entry.name);
          }
        } else {
          if (ImGui::Button(entry.name.c_str(), ImVec2(buttonSize, buttonSize))) {
            std::string const fullPath = std::string(MODEL_PATH) + "/" + entry.relativePath;
            loadModel(fullPath, entry.name);
          }
        }

        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("%s", entry.name.c_str());
          ImGui::EndTooltip();
        }

        float const lastButtonX2 = ImGui::GetItemRectMax().x;
        float const nextButtonX2 = lastButtonX2 + spacing + buttonSize;
        if (i + 1 < availableModels_.size() && nextButtonX2 < windowVisibleX2) ImGui::SameLine();

        ImGui::PopID();
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

void ModelImportPanel::loadModelIndex() {
  std::string const indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
  std::ifstream f(indexPath);
  if (!f.is_open()) {
    std::cerr << "Failed to open model index: " << indexPath << '\n';
    return;
  }

  try {
    nlohmann::json j;
    f >> j;

    for (const auto& item : j) {
      ModelEntry entry;
      if (!item.contains("name")) continue;
      entry.name = item["name"];

      if (item.contains("screenshot")) {
        std::string const screenshotRel = item["screenshot"];

        std::string const fullScreenshotPath = std::string(MODEL_PATH) + "/glTF/" + entry.name + "/" + screenshotRel;
        entry.screenshotPath = fullScreenshotPath;

        try {
          if ((engineState_ != nullptr) && (engineState_->resourceManager != nullptr)) {
            entry.screenshotTexture = engineState_->resourceManager->loadTexture(fullScreenshotPath, true, false);
            if (entry.screenshotTexture) {
              entry.descriptorSet = ImGui_ImplVulkan_AddTexture(entry.screenshotTexture->getSampler(), entry.screenshotTexture->getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
          }
        } catch (const std::exception& e) {
          std::cerr << "Failed to load screenshot for " << entry.name << ": " << e.what() << '\n';
        }
      }

      if (item.contains("variants") && item["variants"].contains("glTF")) {
        std::string const variantFile = item["variants"]["glTF"];

        entry.relativePath = "glTF/" + entry.name + "/glTF/" + variantFile;
      }

      if (!entry.relativePath.empty()) {
        availableModels_.push_back(entry);
      }
    }
  } catch (const std::exception& e) {
    std::cerr << "Error parsing model index: " << e.what() << '\n';
  }
}

void ModelImportPanel::loadModel(const std::string& fullPath, const std::string& name) {
  try {
    engine::Model::MeshletBuildConfig meshletCfg;
    meshletCfg.maxVertices = static_cast<size_t>(meshletMaxVertices_);
    meshletCfg.maxTriangles = static_cast<size_t>(meshletMaxTriangles_);
    meshletCfg.coneWeight = meshletConeWeight_;
    meshletCfg.maxRadius = meshletMaxRadius_;

    engine::ModelInsertionOptions opts;
    opts.enableTextures = true;
    opts.loadMaterials = true;
    opts.enableMorphTargets = true;
    opts.meshletCfg = meshletCfg;

    if ((engineState_ != nullptr) && (engineState_->resourceManager != nullptr)) engine::addModelToScene(*engineState_->resourceManager, engineState_->scene, fullPath, name, opts);

    std::cout << "Loaded model: " << fullPath << '\n';
  } catch (const std::exception& e) {
    std::cerr << "Failed to load model: " << e.what() << '\n';
  }
}

}  // namespace engine
