#pragma once

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  /**
   * @brief Base class for UI panels
   */
  class UIPanel
  {
  public:
    virtual ~UIPanel() = default;

    /**
     * @brief Render the panel
     * @param frameInfo Frame information for rendering
     */
    virtual void render(FrameInfo& frameInfo) = 0;

    /**
     * @brief Check if panel is visible
     */
    bool isVisible() const { return visible_; }

    /**
     * @brief Set panel visibility
     */
    void setVisible(bool visible) { visible_ = visible; }

  protected:
    bool visible_ = true;
  };

} // namespace engine
