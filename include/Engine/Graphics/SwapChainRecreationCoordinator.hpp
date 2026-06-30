#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAINRECREATIONCOORDINATOR_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAINRECREATIONCOORDINATOR_HPP

#include <cstdint>
#include <memory>

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Device.hpp"
#include "Engine/Graphics/SwapChain.hpp"

namespace engine {

    class SwapChainRecreationCoordinator {
       public:
        SwapChainRecreationCoordinator(Window& window, Device& device, uint64_t resizeDebounceMs = 150);

        void                   resetPendingResize();
        [[nodiscard]] uint64_t getPendingResizeTimeNs() const;

        [[nodiscard]] bool shouldDeferRecreationForAcquireOutOfDate() const;
        void               markPendingResizeFromLatestEvent();
        void               refreshPendingResizeIfWindowChanged();
        [[nodiscard]] bool shouldRecreateDeferredResize() const;

        [[nodiscard]] bool handlePresentResult(VkResult result);
        void               onSuccessfulPresent();

        [[nodiscard]] bool waitForOldSwapChainCleanup(const std::shared_ptr<SwapChain>& oldSwapChain, uint64_t timeoutNs) const;

       private:
        Window&  window;
        Device&  device;
        uint64_t pendingResizeTimeNs{0};
        uint64_t resizeDebounceMs{150};
    };

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_SWAPCHAINRECREATIONCOORDINATOR_HPP