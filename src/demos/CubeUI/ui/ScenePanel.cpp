#include "CubeUI/ui/ScenePanel.hpp"

#include <imgui.h>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneUtils.hpp"
#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"
#include "Engine/Systems/AnimationSystem.hpp"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "vulkan/vulkan_core.h"

namespace engine {

  ScenePanel::ScenePanel(Device& device, Scene& scene, AnimationSystem& animationSystem, ResourceManager& resourceManager)
      : device_(device), scene_(scene), animationSystem_(animationSystem), resourceManager_(resourceManager)
  {}

  void ScenePanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::Begin("Scene Objects", &visible_))
    {
      // Temporary auto-test: if AUTO_ADD_MODEL env var is set, auto-load that model once.
      static bool autoTestDone = false;
      if (!autoTestDone)
      {
        const char* autoModelEnv = std::getenv("AUTO_ADD_MODEL");
        if (autoModelEnv && autoModelEnv[0] != '\0')
        {
          std::string autoModelName(autoModelEnv);
          std::string indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
          try
          {
            std::ifstream f(indexPath);
            if (f.is_open())
            {
              nlohmann::json j;
              f >> j;
              for (const auto& item : j)
              {
                if (!item.contains("name")) continue;
                std::string name = item["name"].get<std::string>();
                if (name != autoModelName) continue;

                std::string relativePath;
                if (item.contains("variants") && item["variants"].contains("glTF"))
                {
                  std::string variantFile = item["variants"]["glTF"].get<std::string>();
                  relativePath            = "glTF/" + name + "/glTF/" + variantFile;
                }

                std::string fullPath = relativePath.empty() ? (std::string(MODEL_PATH) + "/glTF/" + name + "/glTF/" + name + ".gltf") : (std::string(MODEL_PATH) + "/" + relativePath);
                try
                {
                  engine::ModelInsertionOptions opts;
                  opts.enableTextures     = true;
                  opts.loadMaterials      = true;
                  opts.enableMorphTargets = true;

                  engine::addModelToScene(resourceManager_, scene_, fullPath, name, opts);
                  std::cerr << "AUTO_ADD_MODEL: loaded and added model: " << fullPath << '\n';
                }
                catch (const std::exception& e)
                {
                  std::cerr << "AUTO_ADD_MODEL exception: " << e.what() << '\n';
                }
                break;
              }
            }
            else
            {
              std::cerr << "AUTO_ADD_MODEL: failed to open model index: " << indexPath << '\n';
            }
          }
          catch (const std::exception& e)
          {
            std::cerr << "AUTO_ADD_MODEL: error parsing index: " << e.what() << '\n';
          }
        }
        autoTestDone = true;
      }

      // Add buttons moved into category headers (see Cameras / Lights sections below)

      ImGui::Separator();

      auto view = scene_.getRegistry().view<entt::entity>();
      ImGui::Text("Total: %zu", view.size());

      std::vector<entt::entity> cameras;
      std::vector<entt::entity> dirLights;
      std::vector<entt::entity> pointLights;
      std::vector<entt::entity> spotLights;
      std::vector<entt::entity> models;

      cameras.reserve(view.size());
      dirLights.reserve(view.size());
      pointLights.reserve(view.size());
      spotLights.reserve(view.size());
      models.reserve(view.size());

      for (auto entity : view)
      {
        if (scene_.getRegistry().all_of<CameraComponent>(entity))
        {
          cameras.push_back(entity);
          continue;
        }
        if (scene_.getRegistry().all_of<DirectionalLightComponent>(entity))
        {
          dirLights.push_back(entity);
          continue;
        }
        if (scene_.getRegistry().all_of<PointLightComponent>(entity))
        {
          pointLights.push_back(entity);
          continue;
        }
        if (scene_.getRegistry().all_of<SpotLightComponent>(entity))
        {
          spotLights.push_back(entity);
          continue;
        }
        if (scene_.getRegistry().all_of<ModelComponent>(entity))
        {
          models.push_back(entity);
          continue;
        }
      }

      auto drawEntityRow = [&](entt::entity entity, const char* icon, ImVec4 color) {
        auto const id = static_cast<uint32_t>(entity);

        // ImGui::PushID takes an int; avoid implementation-defined narrowing from uint32_t.
        assert(id <= static_cast<uint32_t>(std::numeric_limits<int>::max()));
        ImGui::PushID(static_cast<int>(id));

        std::string label = "Object " + std::to_string(id);
        if (scene_.getRegistry().all_of<NameComponent>(entity))
        {
          label = scene_.getRegistry().get<NameComponent>(entity).name + " " + std::to_string(id);
        }

        bool const isSelected = (frameInfo.selectedEntity == entity);

        ImGui::TextColored(color, "%s", icon);
        ImGui::SameLine();

        ImGuiStyle const& style        = ImGui::GetStyle();
        float             actionsWidth = 0.0f;

        auto actionWidthForText = [&](const char* text) { return ImGui::CalcTextSize(text).x + (style.FramePadding.x * 2.0f); };

        // Reserve space for per-row actions so the label becomes a clickable row.
        if (scene_.getRegistry().all_of<CameraComponent>(entity))
        {
          if (entity == frameInfo.cameraEntity)
          {
            actionsWidth += ImGui::CalcTextSize("Active").x;
          }
          else
          {
            actionsWidth += actionWidthForText("Set Active");
          }
          actionsWidth += style.ItemSpacing.x;
        }

        if (entity == frameInfo.cameraEntity)
        {
          actionsWidth += ImGui::CalcTextSize("Delete").x;
        }
        else
        {
          actionsWidth += actionWidthForText("Delete");
        }

        float const selectableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - actionsWidth);
        if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(selectableWidth, 0.0f)))
        {
          frameInfo.selectedObjectId = id;
          frameInfo.selectedEntity   = entity;
        }
        ImGui::SameLine();

        if (scene_.getRegistry().all_of<CameraComponent>(entity))
        {
          if (entity == frameInfo.cameraEntity)
          {
            ImGui::TextDisabled("Active");
          }
          else
          {
            if (ImGui::SmallButton("Set Active"))
            {
              frameInfo.cameraEntity = entity;
            }
          }
          ImGui::SameLine();
        }

        if (entity == frameInfo.cameraEntity)
        {
          ImGui::TextDisabled("Delete");
          if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cannot delete the active camera");
        }
        else
        {
          if (ImGui::SmallButton("Delete"))
          {
            toDelete_.push_back(entity);
          }
        }

        ImGui::PopID();
      };

      ImGuiTreeNodeFlags const rootFlags = ImGuiTreeNodeFlags_DefaultOpen;

      {
        std::string const header = "Cameras (" + std::to_string(cameras.size()) + ")";
        ImGui::PushID("cameras_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        bool const        open  = ImGui::TreeNodeEx("##cameras", rootFlags, "%s", header.c_str());
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (ImGui::SmallButton("+##add_camera"))
        {
          auto entity = scene_.createEntity();
          scene_.getRegistry().emplace<TransformComponent>(entity);
          scene_.getRegistry().emplace<CameraComponent>(entity);
          scene_.getRegistry().emplace<NameComponent>(entity, "Camera");
        }
        ImGui::PopID();
        if (open)
        {
          for (auto entity : cameras)
          {
            drawEntityRow(entity, "[CAM]", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
          }
          ImGui::TreePop();
        }
      }

      {
        size_t const      lightsTotal = dirLights.size() + pointLights.size() + spotLights.size();
        std::string const header      = "Lights (" + std::to_string(lightsTotal) + ")";
        ImGui::PushID("lights_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + (style.FramePadding.x * 2.0f);
        bool const        open  = ImGui::TreeNodeEx("##lights", rootFlags, "%s", header.c_str());
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (ImGui::SmallButton("+##add_light"))
        {
          ImGui::OpenPopup("AddLightPopup");
        }
        if (ImGui::BeginPopup("AddLightPopup"))
        {
          if (ImGui::Selectable("Add Directional"))
          {
            auto entity = scene_.createEntity();
            scene_.getRegistry().emplace<TransformComponent>(entity);
            scene_.getRegistry().emplace<DirectionalLightComponent>(entity);
            scene_.getRegistry().emplace<NameComponent>(entity, "Directional Light");
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::Selectable("Add Point"))
          {
            auto entity = scene_.createEntity();
            scene_.getRegistry().emplace<TransformComponent>(entity);
            scene_.getRegistry().emplace<PointLightComponent>(entity);
            scene_.getRegistry().emplace<NameComponent>(entity, "Point Light");
            ImGui::CloseCurrentPopup();
          }
          if (ImGui::Selectable("Add Spot"))
          {
            auto entity = scene_.createEntity();
            scene_.getRegistry().emplace<TransformComponent>(entity);
            scene_.getRegistry().emplace<SpotLightComponent>(entity);
            scene_.getRegistry().emplace<NameComponent>(entity, "Spot Light");
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }
        ImGui::PopID();
        if (open)
        {
          if (ImGui::TreeNodeEx("##dirlights", rootFlags, "Directional (%zu)", dirLights.size()))
          {
            for (auto entity : dirLights)
            {
              drawEntityRow(entity, "[DIR]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }
            ImGui::TreePop();
          }
          if (ImGui::TreeNodeEx("##pointlights", rootFlags, "Point (%zu)", pointLights.size()))
          {
            for (auto entity : pointLights)
            {
              drawEntityRow(entity, "[PNT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }
            ImGui::TreePop();
          }
          if (ImGui::TreeNodeEx("##spotlights", rootFlags, "Spot (%zu)", spotLights.size()))
          {
            for (auto entity : spotLights)
            {
              drawEntityRow(entity, "[SPT]", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            }
            ImGui::TreePop();
          }
          ImGui::TreePop();
        }
      }

      {
        std::string const header = "Models (" + std::to_string(models.size()) + ")";
        ImGui::PushID("models_header");
        ImGuiStyle const& style = ImGui::GetStyle();
        float const       btnW  = ImGui::CalcTextSize("+").x + style.FramePadding.x * 2.0f;
        bool const        open  = ImGui::TreeNodeEx("##models", rootFlags, "%s", header.c_str());
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - btnW);
        if (ImGui::SmallButton("+##add_model"))
        {
          ImGui::OpenPopup("AddModelPopup");
        }

        // Popup: list available models from model-index.json
        if (ImGui::BeginPopup("AddModelPopup"))
        {
          static char filter[128] = "";
          ImGui::InputText("Filter", filter, sizeof(filter));
          ImGui::Separator();

          std::string const indexPath = std::string(MODEL_PATH) + "/glTF/model-index.json";
          try
          {
            std::ifstream f(indexPath);
            if (!f.is_open())
            {
              ImGui::Text("Failed to open model index: %s", indexPath.c_str());
            }
            else
            {
              nlohmann::json j;
              f >> j;
              for (const auto& item : j)
              {
                if (!item.contains("name")) continue;
                std::string name = item["name"].get<std::string>();

                // Build glTF relative path if present
                std::string relativePath;
                if (item.contains("variants") && item["variants"].contains("glTF"))
                {
                  std::string variantFile = item["variants"]["glTF"].get<std::string>();
                  relativePath            = "glTF/" + name + "/glTF/" + variantFile;
                }

                if (filter[0] != '\0')
                {
                  std::string lowName   = name;
                  std::string lowFilter = filter;
                  std::transform(lowName.begin(), lowName.end(), lowName.begin(), ::tolower);
                  std::transform(lowFilter.begin(), lowFilter.end(), lowFilter.begin(), ::tolower);
                  if (lowName.find(lowFilter) == std::string::npos) continue;
                }

                ImGui::Selectable(name.c_str());
                if (ImGui::IsItemActivated())
                {
                  // Create entity and load model via ResourceManager
                  std::string fullPath = relativePath.empty() ? (std::string(MODEL_PATH) + "/glTF/" + name + "/glTF/" + name + ".gltf") : (std::string(MODEL_PATH) + "/" + relativePath);
                  try
                  {
                    engine::ModelInsertionOptions opts;
                    opts.enableTextures     = true;
                    opts.loadMaterials      = true;
                    opts.enableMorphTargets = true;

                    engine::addModelToScene(resourceManager_, scene_, fullPath, name, opts);
                    ImGui::CloseCurrentPopup();
                  }
                  catch (const std::exception& e)
                  {
                    std::cerr << "Exception loading model: " << e.what() << '\n';
                  }
                }
              }
            }
          }
          catch (const std::exception& e)
          {
            ImGui::Text("Error parsing model index: %s", e.what());
          }

          ImGui::EndPopup();
        }

        ImGui::PopID();
        if (open)
        {
          for (auto entity : models)
          {
            drawEntityRow(entity, "[MDL]", ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
          }
          ImGui::TreePop();
        }
      }
    }
    ImGui::End();
  }

  void ScenePanel::processDelayedDeletions(entt::entity& selectedEntity, uint32_t& selectedObjectId)
  {
    if (toDelete_.empty()) return;

    vkDeviceWaitIdle(device_.device());

    for (auto entity : toDelete_)
    {
      if (entity == selectedEntity)
      {
        selectedEntity   = entt::null;
        selectedObjectId = 0;
      }
      scene_.destroyEntity(entity);
    }
    toDelete_.clear();
  }

} // namespace engine
