#include "3dEngine/systems/ObjectSelectionSystem.hpp"

#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/GameObject.hpp"
#include "3dEngine/Keyboard.hpp"

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
        frameInfo.selectedObjectId = 0;
        frameInfo.selectedObject   = nullptr;
        cameraKeyWasPressed_       = true;
      }
    }
    else
    {
      cameraKeyWasPressed_ = false;
    }

    // U: Previous object
    if (isKeyPressed(mappings.selectPrevious))
    {
      if (!prevKeyWasPressed_)
      {
        // Find previous object in the map
        GameObject::id_t prevId = 0;
        bool             found  = false;

        for (const auto& [id, obj] : frameInfo.gameObjects)
        {
          if (id == frameInfo.selectedObjectId)
          {
            found = true;
            break;
          }
          prevId = id;
        }

        if (found && prevId != 0)
        {
          frameInfo.selectedObjectId = prevId;
          frameInfo.selectedObject   = &frameInfo.gameObjects.at(prevId);
        }
        else
        {
          // If at beginning or camera selected, wrap to last object
          if (!frameInfo.gameObjects.empty())
          {
            auto lastIt                = std::prev(frameInfo.gameObjects.end());
            frameInfo.selectedObjectId = lastIt->first;
            frameInfo.selectedObject   = &lastIt->second;
          }
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
        // If camera is selected (id=0), go to first object
        if (frameInfo.selectedObjectId == 0)
        {
          if (!frameInfo.gameObjects.empty())
          {
            auto firstIt               = frameInfo.gameObjects.begin();
            frameInfo.selectedObjectId = firstIt->first;
            frameInfo.selectedObject   = &firstIt->second;
          }
        }
        else
        {
          // Find current object and move to next
          auto it = frameInfo.gameObjects.find(frameInfo.selectedObjectId);
          if (it != frameInfo.gameObjects.end())
          {
            ++it;
            if (it != frameInfo.gameObjects.end())
            {
              frameInfo.selectedObjectId = it->first;
              frameInfo.selectedObject   = &it->second;
            }
            else
            {
              // Wrap around to camera
              frameInfo.selectedObjectId = 0;
              frameInfo.selectedObject   = nullptr;
            }
          }
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
