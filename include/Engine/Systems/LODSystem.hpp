#ifndef VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LODSYSTEM_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LODSYSTEM_HPP

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  class LODSystem
  {
  public:
    LODSystem() = default;

    static void update(FrameInfo& frameInfo);
  };

} // namespace engine

#endif // VULKANENGINE_INCLUDE_ENGINE_SYSTEMS_LODSYSTEM_HPP
