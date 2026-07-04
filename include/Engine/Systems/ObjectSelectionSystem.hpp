#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_OBJECTSELECTIONSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_OBJECTSELECTIONSYSTEM_HPP
#include "Engine/Core/Keyboard.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
namespace engine {
    class ObjectSelectionSystem {
       public:
        explicit ObjectSelectionSystem(Keyboard& keyboard);
        void update(FrameInfo& frameInfo);

       private:
        Keyboard&          keyboard_;
        bool               nextKeyWasPressed_   = false;
        bool               prevKeyWasPressed_   = false;
        bool               cameraKeyWasPressed_ = false;
        [[nodiscard]] bool isKeyPressed(int key) const {
            return keyboard_.isKeyPressed(key);
        }
    };
}  // namespace engine
#endif
