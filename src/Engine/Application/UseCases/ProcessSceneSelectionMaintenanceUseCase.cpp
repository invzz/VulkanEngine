#include "Engine/Application/UseCases/ProcessSceneSelectionMaintenanceUseCase.hpp"

namespace engine {

ProcessSceneSelectionMaintenanceUseCase::ProcessSceneSelectionMaintenanceUseCase(
    ISceneSelectionMaintenancePort& selectionMaintenance)
    : selectionMaintenance_(selectionMaintenance) {}

void ProcessSceneSelectionMaintenanceUseCase::execute(SceneRuntimeState& runtimeState) const {
  selectionMaintenance_.processSelectionMaintenance(runtimeState);
}

}  // namespace engine