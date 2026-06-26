#ifndef VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_DISPLAY_HPP
#define VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_DISPLAY_HPP

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/Descriptors.hpp"
#include "Engine/Graphics/Pipeline.hpp"

namespace engine {

    class ViewportTexture;

    /**
     * @brief Renders the viewport texture to the swap chain with tone mapping.
     *
     * Creates a descriptor set for the viewport texture, a fullscreen quad
     * pipeline, and a render pass matching the swap chain format.
     * Renders the viewport scene to the swap chain so the CompositionPass
     * can overlay UI on top.
     */
    class ViewportDisplay {
    public:
        ViewportDisplay() = default;
        ~ViewportDisplay();

        ViewportDisplay(const ViewportDisplay&)            = delete;
        ViewportDisplay& operator=(const ViewportDisplay&) = delete;

        /**
         * @brief Initialize the display infrastructure.
         * @param device Vulkan device
         * @param swapChainRenderPass The swap chain's render pass (for reference)
         * @param swapChainFormat Format of the swap chain images
         * @param swapChainExtent Extent of the swap chain
         */
        void initialize(Device& device,
                        VkRenderPass swapChainRenderPass,
                        VkFormat swapChainFormat,
                        VkExtent2D swapChainExtent);

        /**
         * @brief Set the viewport texture to display.
         */
        void setViewportTexture(const ViewportTexture& viewportTexture);

        /**
         * @brief Update the render pass handle (used during swapchain recreation).
         */
        void setRenderPass(VkRenderPass renderPass);

        /**
         * @brief Render the viewport texture to the swap chain.
         * @param commandBuffer Command buffer to record to
         * @param swapChainFramebuffer Swap chain framebuffer for the current image
         */
        void execute(VkCommandBuffer commandBuffer, VkFramebuffer swapChainFramebuffer) const;

        /**
         * @brief Check if this instance has been initialized.
         */
        [[nodiscard]] bool isValid() const {
            return displayRenderPass_ != VK_NULL_HANDLE &&
                   descriptorPool_ != VK_NULL_HANDLE &&
                   descriptorSet_ != VK_NULL_HANDLE &&
                   pipelineLayout_ != VK_NULL_HANDLE &&
                   pipeline_ != nullptr;
        }

        /**
         * @brief Get the current viewport texture pointer (for validation).
         */
        [[nodiscard]] const ViewportTexture* getViewportTexture() const {
            return viewportTexture_;
        }

    private:
        void createRenderPass(Device& device);
        void createDescriptorInfrastructure(Device& device);
        void createPipeline(Device& device);

        Device*                    device_             = nullptr;
        VkRenderPass               swapChainRenderPass_ = VK_NULL_HANDLE;
        VkFormat                   swapChainFormat_     = VK_FORMAT_UNDEFINED;
        VkExtent2D                 swapChainExtent_     = {0, 0};

        // Display-specific render pass (no clear, matches swap chain format)
        VkRenderPass               displayRenderPass_   = VK_NULL_HANDLE;

        // Descriptor infrastructure
        std::unique_ptr<DescriptorSetLayout> descriptorSetLayoutObj_;
        VkDescriptorSetLayout                descriptorSetLayout_  = VK_NULL_HANDLE;
        VkDescriptorPool                     descriptorPool_       = VK_NULL_HANDLE;
        VkDescriptorSet                      descriptorSet_        = VK_NULL_HANDLE;
        VkPipelineLayout                     pipelineLayout_       = VK_NULL_HANDLE;

        // Pipeline
        std::unique_ptr<Pipeline>  pipeline_;

        // Viewport texture info
        const ViewportTexture*     viewportTexture_      = nullptr;
    };

}  // namespace engine

#endif  // VULKANENGINE_ENGINE_GRAPHICS_VIEWPORT_DISPLAY_HPP
