#include "ScenePanel.hpp"

#include <imgui.h>

#include <iostream>
#include <string>

#include "Engine/Scene/components/ModelComponent.hpp"
#include "Engine/Scene/components/NameComponent.hpp"

namespace engine {

  ScenePanel::ScenePanel(Device& device, Scene& scene, AnimationSystem& animationSystem) : device_(device), scene_(scene), animationSystem_(animationSystem) {}

  void ScenePanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::Begin("Scene Objects", &visible_))
    {
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

        if (ImGui::SmallButton("Delete"))
        {
          toDelete_.push_back(entity);
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
