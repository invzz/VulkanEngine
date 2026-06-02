#pragma once

#include "Engine/Application/Ports/ISceneSelectionMaintenancePort.hpp"

namespace engine {

class ProcessSceneSelectionMaintenanceUseCase {
 public:
  explicit ProcessSceneSelectionMaintenanceUseCase(ISceneSelectionMaintenancePort& selectionMaintenance);

  void execute(SceneRuntimeState& runtimeState) const;

 private:
  ISceneSelectionMaintenancePort& selectionMaintenance_;
};

}  // namespace engine