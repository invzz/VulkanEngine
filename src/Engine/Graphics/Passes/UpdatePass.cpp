
#include "Engine/Graphics/Passes/UpdatePass.hpp"

#include "Engine/EngineState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Systems/CameraSystem.hpp"
#include "Engine/Systems/LODSystem.hpp"

namespace engine {

void UpdatePass::execute(FrameInfo& frameInfo) {
  // forward update calls to systems owned by EngineState
  if (engineState_->objectSelectionSystem) engineState_->objectSelectionSystem->update(frameInfo);
  if (engineState_->inputSystem) engineState_->inputSystem->update(frameInfo);
  LODSystem::update(frameInfo);
  CameraSystem::update(frameInfo, renderer.getAspectRatio());
}

}  // namespace engine