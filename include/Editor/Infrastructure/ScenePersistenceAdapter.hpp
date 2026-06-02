#pragma once

#include <string>

#include "Engine/Application/Ports/IScenePersistencePort.hpp"

namespace engine {

class SceneSerializer;

class ScenePersistenceAdapter final : public IScenePersistencePort {
 public:
  explicit ScenePersistenceAdapter(SceneSerializer& serializer);

  void saveScene(const std::string& path) override;
  bool loadScene(const std::string& path) override;

 private:
  SceneSerializer& serializer_;
};

}  // namespace engine
