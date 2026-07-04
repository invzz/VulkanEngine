#include "Engine/Graphics/SwapChainRecreationCoordinator.hpp"

#include <chrono>
#include <thread>

#include "Engine/Core/Logger.hpp"
namespace engine {
    SwapChainRecreationCoordinator::SwapChainRecreationCoordinator(Window& window, Device& device, uint64_t resizeDebounceMs)
        : window(window), device(device), resizeDebounceMs(resizeDebounceMs) {}
    void SwapChainRecreationCoordinator::resetPendingResize() {
        pendingResizeTimeNs = 0;
    }
    uint64_t SwapChainRecreationCoordinator::getPendingResizeTimeNs() const {
        return pendingResizeTimeNs;
    }
    bool SwapChainRecreationCoordinator::shouldDeferRecreationForAcquireOutOfDate() const {
        const uint64_t nowNs =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        const uint64_t lastResize = window.getLastResizeTimeNs();
        return lastResize != 0 && (nowNs - lastResize) < (resizeDebounceMs * 1000000ULL);
    }
    void SwapChainRecreationCoordinator::markPendingResizeFromLatestEvent() {
        pendingResizeTimeNs = window.getLastResizeTimeNs();
    }
    void SwapChainRecreationCoordinator::refreshPendingResizeIfWindowChanged() {
        if (!window.wasWindowResized()) {
            return;
        }
        const uint64_t newTs = window.getLastResizeTimeNs();
        pendingResizeTimeNs  = newTs;
        (void) window.consumeWindowResized();
        Logger::debug(LogChannel::Render, "updated pending resize timestamp to ", newTs);
    }
    bool SwapChainRecreationCoordinator::shouldRecreateDeferredResize() const {
        return pendingResizeTimeNs != 0 && window.isResizeStable(resizeDebounceMs) && window.getLastResizeTimeNs() == pendingResizeTimeNs;
    }
    bool SwapChainRecreationCoordinator::handlePresentResult(VkResult result) {
        if (result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR) {
            return false;
        }
        if (shouldDeferRecreationForAcquireOutOfDate()) {
            markPendingResizeFromLatestEvent();
            Logger::info(LogChannel::Render,
                "present returned OUT_OF_DATE/SUBOPTIMAL during active resize; deferring recreation until stable (ts=",
                pendingResizeTimeNs,
                ")");
            return false;
        }
        Logger::info(LogChannel::Render, "swapchain present/submit indicates recreation is needed (result=", result, ")");
        return true;
    }
    void SwapChainRecreationCoordinator::onSuccessfulPresent() {
        if (window.consumeWindowResized()) {
            pendingResizeTimeNs = window.getLastResizeTimeNs();
            Logger::info(LogChannel::Render, "scheduled swapchain recreation at ts=", pendingResizeTimeNs);
        }
        refreshPendingResizeIfWindowChanged();
    }
    bool SwapChainRecreationCoordinator::waitForOldSwapChainCleanup(const std::shared_ptr<SwapChain>& oldSwapChain, uint64_t timeoutNs) const {
        if (oldSwapChain == nullptr) {
            return true;
        }
        if (!oldSwapChain->waitForInFlightFences(timeoutNs)) {
            Logger::warn(LogChannel::Sync, "Previous swapchain fences did not signal within timeout; entering watchdog vkDeviceWaitIdle loop");
            const uint64_t maxWaitMs = 5000;
            const uint64_t stepMs    = 200;
            uint64_t       waitedMs  = 0;
            while (waitedMs < maxWaitMs) {
                Logger::warn(LogChannel::Sync, "vkDeviceWaitIdle attempt, waited ", waitedMs, "ms so far");
                vkDeviceWaitIdle(device.device());
                if (oldSwapChain->waitForInFlightFences(0)) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
                waitedMs += stepMs;
            }
            if (!oldSwapChain->waitForInFlightFences(0)) {
                Logger::error(LogChannel::Sync, "Timeout waiting for previous swapchain cleanup after ", maxWaitMs, "ms; aborting swapchain recreation");
                return false;
            }
        }
        const VkResult graphicsIdle = vkQueueWaitIdle(device.graphicsQueue());
        const VkResult presentIdle  = vkQueueWaitIdle(device.presentQueue());
        if (graphicsIdle != VK_SUCCESS || presentIdle != VK_SUCCESS) {
            Logger::warn(LogChannel::Sync,
                "waitForOldSwapChainCleanup: queue idle wait failed (graphics=",
                graphicsIdle,
                ", present=",
                presentIdle,
                ")");
            return false;
        }
        Logger::info(LogChannel::Sync, "Released old swapchain after fences signaled and queues idled.");
        return true;
    }
}  // namespace engine