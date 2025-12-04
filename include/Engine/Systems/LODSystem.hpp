#pragma once

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  class LODSystem
  {
  public:
    LODSystem() = default;

    void update(FrameInfo& frameInfo);
  };

} // namespace engine
