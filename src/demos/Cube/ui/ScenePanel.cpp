#include "ScenePanel.hpp"

#include <imgui.h>

#include <iostream>
#include <string>

#include "Engine/Scene/components/CameraComponent.hpp"
#include "Engine/Scene/components/DirectionalLightComponent.hpp"
#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"
#include "Engine/Scene/components/PointLightComponent.hpp"
#include "Engine/Scene/components/SpotLightComponent.hpp"
#include "Engine/Scene/components/TransformComponent.hpp"

namespace engine {

  ScenePanel::ScenePanel(Device& device, Scene& scene, AnimationSystem& animationSystem) : device_(device), scene_(scene), animationSystem_(animationSystem) {}

  void ScenePanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::Begin("Scene Objects", &visible_))
    {
      if (ImGui::Button("Add Camera"))
      {
        auto entity = scene_.createEntity();
        scene_.getRegistry().emplace<TransformComponent>(entity);
        scene_.getRegistry().emplace<CameraComponent>(entity);
        scene_.getRegistry().emplace<NameComponent>(entity, "Camera");
      }
      ImGui::SameLine();
      if (ImGui::Button("Add Point Light"))
      {
        auto entity = scene_.createEntity();
        scene_.getRegistry().emplace<TransformComponent>(entity);
        scene_.getRegistry().emplace<PointLightComponent>(entity);
        scene_.getRegistry().emplace<NameComponent>(entity, "Point Light");
      }
      ImGui::SameLine();
      if (ImGui::Button("Add Dir Light"))
      {
        auto entity = scene_.createEntity();
        scene_.getRegistry().emplace<TransformComponent>(entity);
        scene_.getRegistry().emplace<DirectionalLightComponent>(entity);
        scene_.getRegistry().emplace<NameComponent>(entity, "Directional Light");
      }
      ImGui::SameLine();
      if (ImGui::Button("Add Spot Light"))
      {
        auto entity = scene_.createEntity();
        scene_.getRegistry().emplace<TransformComponent>(entity);
        scene_.getRegistry().emplace<SpotLightComponent>(entity);
        scene_.getRegistry().emplace<NameComponent>(entity, "Spot Light");
      }

      ImGui::Separator();

      auto view = scene_.getRegistry().view<entt::entity>();
      ImGui::Text("Total: %zu", view.size());

      for (auto entity : view)
      {
        uint32_t id = (uint32_t)entity;

        ImGui::PushID(id);

        std::string label = "Object " + std::to_string(id);
        if (scene_.getRegistry().all_of<NameComponent>(entity))
        {
          label = scene_.getRegistry().get<NameComponent>(entity).name + " " + std::to_string(id);
        }

        if (scene_.getRegistry().all_of<ModelComponent>(entity))
        {
          label += " (Model)";
        }

        ImGui::BulletText("%s", label.c_str());
        ImGui::SameLine();

        if (ImGui::SmallButton("Select"))
        {
          frameInfo.selectedObjectId = id;
          frameInfo.selectedEntity   = entity;
        }
        ImGui::SameLine();

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
