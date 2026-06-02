#pragma once

#include "Engine/Application/SceneRuntimeState.hpp"
#include "Engine/Scene/Scene.hpp"

namespace engine {

class ReconcileSceneLoadUseCase {
 public:
  explicit ReconcileSceneLoadUseCase(Scene& scene);

  void execute(SceneRuntimeState& runtimeState) const;

 private:
  Scene& scene_;
};

}  // namespace engine