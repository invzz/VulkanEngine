#ifndef EDITOR_UIMANAGER_HPP
#define EDITOR_UIMANAGER_HPP

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "Editor/ui/UIPanel.hpp"
#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/ImGuiManager.hpp"

namespace engine {

  /**
   * @brief Manages all UI panels
   */
  class UIManager
  {
  public:
    explicit UIManager(ImGuiManager& imguiManager);

    /**
     * @brief Add a panel to the manager
     */
    void addPanel(std::unique_ptr<UIPanel> panel);

    /**
     * @brief Render all panels
     */
    void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer);
    void render(FrameInfo& frameInfo, VkCommandBuffer commandBuffer, bool drawUI);

    /**
     * @brief Get a specific panel by type (returns nullptr if not found)
     */
    template <typename T> T* getPanel()
    {
      for (auto& panel : panels_)
      {
        if (auto* typed = dynamic_cast<T*>(panel.get()))
        {
          return typed;
        }
      }
      return nullptr;
    }

    void setOnSaveScene(std::function<void()> callback) { onSaveScene_ = std::move(callback); }
    void setOnLoadScene(std::function<void()> callback) { onLoadScene_ = std::move(callback); }

  private:
    ImGuiManager&                         imguiManager_;
    std::vector<std::unique_ptr<UIPanel>> panels_;
    std::function<void()>                 onSaveScene_;
    std::function<void()>                 onLoadScene_;
  };
} // namespace engine

#endif // EDITOR_UIMANAGER_HPP
