#include "Editor/SceneLoader.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Core/Logger.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Scene/Scene.hpp"

#include "Editor/ModelLoadProcessor.hpp"
#include "ModelLib/Resources/ResourceManager.hpp"
#include "entt/entity/fwd.hpp"
namespace engine {
    void SceneLoader::loadScene(Device& device, Scene& scene, ResourceManager& resourceManager) {
        if (!scene.getRegistry().storage<entt::entity>().empty()) {
            return;
        }
    }
    void SceneLoader::createFromFile(Device& device, Scene& scene, ResourceManager& resourceManager, const std::string& modelPath) {
        if (!scene.getRegistry().storage<entt::entity>().empty()) {
            return;
        }
        auto modelPtr = resourceManager.loadModel(modelPath, true, true, true);
        if (!modelPtr) {
            Logger::error(LogChannel::General, "[SceneLoader] Failed to load model: ", modelPath);
            return;
        }
        // Use ModelLoadProcessor to handle all post-load processing
        ModelLoadProcessor::processLoadedModel(
            scene,
            modelPtr,
            modelPath,
            "LoadedModel",
            ModelInsertionOptions::StaticColliderImportMode::AutoDetect);
    }
}  // namespace engine
