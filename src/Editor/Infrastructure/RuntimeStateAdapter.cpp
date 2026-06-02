#include "Editor/Infrastructure/RuntimeStateAdapter.hpp"

#include "Engine/EngineState.hpp"

namespace engine {

RuntimeStateAdapter::RuntimeStateAdapter(EngineState& engineState)
    : engineState_(engineState) {}

bool& RuntimeStateAdapter::showGridRef() {
  return engineState_.showGridRef();
}

PostProcessPushConstants& RuntimeStateAdapter::postProcessPushRef() {
  return engineState_.postProcessPushRef();
}

}  // namespace engine
