#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class EngineState;
class Renderer;
class FrameInfo;

class UpdatePass : public IRenderPass {
 public:
  UpdatePass(EngineState* engineState, Renderer& renderer) : engineState_(engineState), renderer(renderer) {}

  [[nodiscard]] std::string& getName() const override {
    static std::string name = "Update";
    return name;
  }
  void execute(FrameInfo& frameInfo) override;

 private:
  EngineState* engineState_ = nullptr;
  Renderer& renderer;
};

}  // namespace engine