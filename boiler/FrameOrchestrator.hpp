#ifndef FRAME_ORCHESTRATOR_HPP
#define FRAME_ORCHESTRATOR_HPP

#include "Engine/Core/Window.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "RenderPipeline.hpp"

namespace engine {
class FrameOrchestrator {
 public:
  FrameOrchestrator(Window&, Renderer&, RenderPipeline&);

  void run();

 private:
  void update(float dt);
  void render(float dt);
};
}  // namespace engine
#endif  // FRAME_ORCHESTRATOR_HPP