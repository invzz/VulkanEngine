#pragma once

#include <string>

#include "Engine/Application/Ports/IScenePersistencePort.hpp"

namespace engine {

    class SaveSceneUseCase {
       public:
        explicit SaveSceneUseCase(IScenePersistencePort& scenePersistence);

        [[nodiscard]] bool execute(const std::string& path) const;

       private:
        IScenePersistencePort& scenePersistence_;
    };

}  // namespace engine
