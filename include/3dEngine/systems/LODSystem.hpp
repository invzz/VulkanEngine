#pragma once

#include "3dEngine/FrameInfo.hpp"

namespace engine {

  class LODSystem
  {
  public:
    LODSystem() = default;

    void update(FrameInfo& frameInfo);
  };

} // namespace engine
