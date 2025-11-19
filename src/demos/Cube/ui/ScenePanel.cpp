#include "ScenePanel.hpp"

#include <imgui.h>

#include <iostream>
#include <string>

namespace engine {

  ScenePanel::ScenePanel(Device& device, GameObject::Map& gameObjects, AnimationSystem& animationSystem)
      : device_(device), gameObjects_(gameObjects), animationSystem_(animationSystem)
  {}

  void ScenePanel::render(FrameInfo& frameInfo)
  {
    if (!visible_) return;

    if (ImGui::CollapsingHeader("Scene Objects"))
    {
      ImGui::Text("Total: %zu", gameObjects_.size());

      toDelete_.clear();

      for (auto& [id, obj] : gameObjects_)
      {
        ImGui::PushID(id);

        std::string label = obj.getName() + " " + std::to_string(id);

        if (obj.model)
        {
          label += " (Model)";
        }

        ImGui::BulletText("%s", label.c_str());
        ImGui::SameLine();

        if (ImGui::SmallButton("Select"))
        {
          frameInfo.selectedObjectId = id;
          frameInfo.selectedObject   = &obj;
        }
        ImGui::SameLine();

        if (ImGui::SmallButton("Delete"))
        {
          toDelete_.push_back(id);
        }

        ImGui::PopID();
      }

      // Mark objects for deferred deletion
      for (auto id : toDelete_)
      {
        animationSystem_.unregisterAnimatedObject(id);
        pendingDeletions_.push_back({id, FRAMES_TO_WAIT});
        std::cout << "[ScenePanel] Marked object " << id << " for deletion in " << FRAMES_TO_WAIT << " frames" << std::endl;
      }
    }
  }

  void ScenePanel::processDelayedDeletions()
  {
    // Process pending deletions - this should be called between frames, not during rendering
    for (auto it = pendingDeletions_.begin(); it != pendingDeletions_.end();)
    {
      it->framesRemaining--;
      if (it->framesRemaining <= 0)
      {
        // Wait for GPU to finish all work before deleting
        device_.WaitIdle();
        gameObjects_.erase(it->id);
        std::cout << "[ScenePanel] Deleted object " << it->id << std::endl;
        it = pendingDeletions_.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

} // namespace engine
