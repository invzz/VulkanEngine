#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

#include <iostream>
#include <memory>
#include <utility>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

void RenderGraph::addPass(std::unique_ptr<IRenderPass> pass) {
  passes.push_back(std::move(pass));
}

void RenderGraph::execute(FrameInfo& frameInfo) {
  for (auto& pass : passes) {
    pass->execute(frameInfo);
  }
}

void RenderGraph::reset() {
  passes.clear();
}

}  // namespace engine
