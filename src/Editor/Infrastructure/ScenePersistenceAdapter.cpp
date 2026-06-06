#include "Editor/Infrastructure/ScenePersistenceAdapter.hpp"

#include "EngineSceneIO/Scene/SceneSerializer.hpp"

namespace engine {

    ScenePersistenceAdapter::ScenePersistenceAdapter(SceneSerializer& serializer)
        : serializer_(serializer) {}

    void ScenePersistenceAdapter::saveScene(const std::string& path) {
        serializer_.serialize(path);
    }

    bool ScenePersistenceAdapter::loadScene(const std::string& path) {
        return serializer_.deserialize(path);
    }

}  // namespace engine
