#include "Engine/Graphics/RenderGraph.hpp"

#include <memory>
#include <utility>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

  void RenderGraph::addPass(std::unique_ptr<RenderPass> pass)
  {
    passes.push_back(std::move(pass));
  }

  void RenderGraph::execute(FrameInfo& frameInfo)
  {
    for (auto& pass : passes)
    {
      pass->execute(frameInfo);
    }
  }

  void RenderGraph::reset()
  {
    passes.clear();
  }

} // namespace engine
