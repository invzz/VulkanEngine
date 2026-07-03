#include "Engine/Graphics/Viewport.hpp"

#include <imgui_impl_vulkan.h>

#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

    void Viewport::create(Device& device, Renderer& renderer) {
        registerAllFrames(device, renderer);
    }

    void Viewport::resize(Device& device, Renderer& renderer, VkExtent2D newExtent) {
        renderer.resizeOffscreenFramebuffer(newExtent);

        registerAllFrames(device, renderer);
    }

    void Viewport::registerAllFrames(Device& device, Renderer& renderer) {
        const int maxFrames = SwapChain::maxFramesInFlight();
        for (int i = 0; i < maxFrames; ++i) {
            VkImageView imageView = renderer.getOffscreenColorImageView(i);
            VkSampler   sampler   = renderer.getOffscreenColorSampler(i);

            if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) {
                imTextureIDs_[static_cast<size_t>(i)] = (ImTextureID) nullptr;
                continue;
            }

            imTextureIDs_[static_cast<size_t>(i)] = reinterpret_cast<ImTextureID>(
                ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        }
    }

}  // namespace engine
