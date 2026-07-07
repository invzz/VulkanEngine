#ifndef CUBE_SCENELOADER_HPP
#define CUBE_SCENELOADER_HPP
#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"

#include "ModelLib/Resources/ResourceManager.hpp"
namespace engine {
    class SceneLoader {
       public:
        static void loadScene(Device& device, Scene& scene, ResourceManager& resourceManager);
        static void createFromFile(Device& device, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath);

       private:
    };
}  // namespace engine
#endif
