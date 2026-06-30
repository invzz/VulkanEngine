#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENT_HPP

namespace engine {

    class GameObject;

    class Component {
       public:
        virtual ~Component() = default;

        [[nodiscard]] GameObject* getOwner() const {
            return owner;
        }
        void setOwner(GameObject* newOwner) {
            owner = newOwner;
        }

       protected:
        GameObject* owner = nullptr;
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENT_HPP
