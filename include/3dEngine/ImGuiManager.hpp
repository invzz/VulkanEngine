#pragma once

#include <vulkan/vulkan.h>

#include "Device.hpp"
#include "Window.hpp"

namespace engine {

  /**
   * @brief Manager for ImGui integration with Vulkan
   *
   * Handles ImGui initialization, rendering, and cleanup.
   * Call init() once during setup, then newFrame() before UI code,
   * and render() to draw the UI.
   */
  class ImGuiManager
  {
  public:
    ImGuiManager(Window& window, Device& device, VkRenderPass renderPass, uint32_t imageCount);
    ~ImGuiManager();

    ImGuiManager(const ImGuiManager&)            = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

    /**
     * @brief Start a new ImGui frame
     * Call this before any ImGui UI code
     */
    void newFrame();

    /**
     * @brief Render ImGui draw data to command buffer
     * Call this inside render pass after your scene rendering
     */
    void render(VkCommandBuffer commandBuffer);

    /**
     * @brief Update ImGui fonts/resources
     * Call this after window resize
     */
    void updateAfterResize();

  private:
    void initImGui();
    void setupVulkanBackend(uint32_t imageCount);

    Window&          window_;
    Device&          device_;
    VkRenderPass     renderPass_;
    VkDescriptorPool imguiDescriptorPool_{VK_NULL_HANDLE};
  };

} // namespace engine
