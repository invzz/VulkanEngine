#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

#include <memory>
#include <utility>

#include "Engine/Graphics/FrameInfo.hpp"
#include "Engine/Graphics/GpuProfiler.hpp"

namespace engine {

    void RenderGraph::addPass(std::unique_ptr<IRenderPass> pass) {
        passes.push_back(std::move(pass));
    }

    void RenderGraph::execute(FrameInfo& frameInfo) {
        auto& profiler = GpuProfiler::instance();
        if (profiler.isEnabled()) {
            profiler.beginFrame(static_cast<uint64_t>(frameInfo.frameIndex), frameInfo.commandBuffer);
        }

        for (auto& pass : passes) {
            if (profiler.isEnabled()) {
                profiler.beginPass(pass->getName());
            }

            try {
                pass->execute(frameInfo);
            } catch (...) {
                if (profiler.isEnabled()) {
                    profiler.endPass();
                    profiler.endFrame();
                }
                throw;
            }

            if (profiler.isEnabled()) {
                profiler.endPass();
            }
        }

        if (profiler.isEnabled()) {
            profiler.endFrame();
        }
    }

    void RenderGraph::reset() {
        passes.clear();
    }

}  // namespace engine
