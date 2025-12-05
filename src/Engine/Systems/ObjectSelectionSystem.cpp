#include "Engine/Systems/ObjectSelectionSystem.hpp"

#include <algorithm>
#include <vector>

#include "Engine/Core/Keyboard.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

  ObjectSelectionSystem::ObjectSelectionSystem(Keyboard& keyboard) : keyboard_{keyboard} {}

  void ObjectSelectionSystem::update(FrameInfo& frameInfo)
  {
    const auto& mappings = keyboard_.mappings;

    // C: Select camera (check first to avoid conflicts)
    if (isKeyPressed(mappings.selectCamera))
    {
      if (!cameraKeyWasPressed_)
      {
        frameInfo.selectedObjectId = (uint32_t)frameInfo.cameraEntity;
        frameInfo.selectedEntity   = frameInfo.cameraEntity;
        cameraKeyWasPressed_       = true;
      }
    }
    else
    {
      cameraKeyWasPressed_ = false;
    }

    // Helper to get sorted entities
    std::vector<entt::entity> entities;
    auto                      view = frameInfo.scene->getRegistry().view<entt::entity>();
    for (auto entity : view)
    {
      entities.push_back(entity);
    }
    std::sort(entities.begin(), entities.end());

    if (entities.empty()) return;

    // U: Previous object
    if (isKeyPressed(mappings.selectPrevious))
    {
      if (!prevKeyWasPressed_)
      {
        auto it = std::find(entities.begin(), entities.end(), (entt::entity)frameInfo.selectedObjectId);

        if (it != entities.end() && it != entities.begin())
        {
          // Select previous
          auto prev                  = std::prev(it);
          frameInfo.selectedObjectId = (uint32_t)*prev;
          frameInfo.selectedEntity   = *prev;
        }
        else
        {
          // Wrap to last (from first or from camera/unknown)
          auto last                  = entities.back();
          frameInfo.selectedObjectId = (uint32_t)last;
          frameInfo.selectedEntity   = last;
        }
        prevKeyWasPressed_ = true;
      }
    }
    else
    {
      prevKeyWasPressed_ = false;
    }

    // Y: Next object
    if (isKeyPressed(mappings.selectNext))
    {
      if (!nextKeyWasPressed_)
      {
        auto it = std::find(entities.begin(), entities.end(), (entt::entity)frameInfo.selectedObjectId);

        if (it != entities.end() && std::next(it) != entities.end())
        {
          // Select next
          auto next                  = std::next(it);
          frameInfo.selectedObjectId = (uint32_t)*next;
          frameInfo.selectedEntity   = *next;
        }
        else
        {
          // Wrap to first (from last or from camera/unknown)
          auto first                 = entities.front();
          frameInfo.selectedObjectId = (uint32_t)first;
          frameInfo.selectedEntity   = first;
        }
        nextKeyWasPressed_ = true;
      }
    }
    else
    {
      nextKeyWasPressed_ = false;
    }
  }

} // namespace engine
