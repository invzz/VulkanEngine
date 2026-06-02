#pragma once

#include <string>

#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"
#include "Engine/Application/Ports/IScenePersistencePort.hpp"
#include "Engine/Application/SceneRuntimeState.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

class LoadSceneUseCase {
 public:
  LoadSceneUseCase(Scene& scene, IScenePersistencePort& scenePersistence, IPhysicsRuntimePort* physicsRuntime);

  bool execute(const std::string& path, SceneRuntimeState& runtimeState) const;

 private:
  void ensureCameraExists(entt::entity& cameraEntity) const;

  Scene& scene_;
  IScenePersistencePort& scenePersistence_;
  IPhysicsRuntimePort* physicsRuntime_ = nullptr;
};

}  // namespace engine
