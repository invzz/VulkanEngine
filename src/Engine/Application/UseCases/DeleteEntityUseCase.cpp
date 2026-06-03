#include "Engine/Application/UseCases/DeleteEntityUseCase.hpp"

namespace engine {

DeleteEntityUseCase::DeleteEntityUseCase(ISceneEntityPort& sceneEntity)
    : sceneEntity_(sceneEntity) {}

bool DeleteEntityUseCase::execute(entt::entity entity) {
  if (!sceneEntity_.isValid(entity)) {
    return false;
  }
  sceneEntity_.deleteEntity(entity);
  return true;
}

}  // namespace engine
