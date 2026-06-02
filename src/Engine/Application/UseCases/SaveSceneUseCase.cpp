#include "Engine/Application/UseCases/SaveSceneUseCase.hpp"

#include <exception>

namespace engine {

SaveSceneUseCase::SaveSceneUseCase(IScenePersistencePort& scenePersistence)
    : scenePersistence_(scenePersistence) {}

bool SaveSceneUseCase::execute(const std::string& path) const {
  try {
    scenePersistence_.saveScene(path);
    return true;
  } catch (const std::exception&) {
    return false;
  } catch (...) {
    return false;
  }
}

}  // namespace engine
