#pragma once

#include <string>

namespace engine {

class IScenePersistencePort {
 public:
  virtual ~IScenePersistencePort() = default;

  virtual void saveScene(const std::string& path) = 0;
  virtual bool loadScene(const std::string& path) = 0;
};

}  // namespace engine
