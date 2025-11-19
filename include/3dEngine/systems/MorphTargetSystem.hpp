#pragma once

#include <memory>

#include "3dEngine/Device.hpp"
#include "3dEngine/FrameInfo.hpp"
#include "3dEngine/MorphTargetManager.hpp"

namespace engine {

  /**
   * @brief System for managing morph target animations
   *
   * Automatically initializes and updates morph target blending for all models.
   * Runs compute shaders to blend base vertices with morph deltas before rendering.
   */
  class MorphTargetSystem
  {
  public:
    MorphTargetSystem(Device& device);
    ~MorphTargetSystem();

    MorphTargetSystem(const MorphTargetSystem&)            = delete;
    MorphTargetSystem& operator=(const MorphTargetSystem&) = delete;

    /**
     * @brief Update morph targets for all models
     *
     * This dispatches compute shaders to blend vertices.
     * Should be called BEFORE the render pass begins.
     *
     * @param frameInfo Contains command buffer and game objects
     */
    void update(FrameInfo& frameInfo);

    /**
     * @brief Get the underlying manager (for use by render systems)
     */
    MorphTargetManager* getManager() { return manager_.get(); }

  private:
    std::unique_ptr<MorphTargetManager> manager_;
  };

} // namespace engine
