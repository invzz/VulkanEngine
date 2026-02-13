#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class EngineState;
class FrameInfo;

class ComputePass : public IRenderPass {
 public:
  explicit ComputePass(EngineState* engineState) : engineState_(engineState) {}

  [[nodiscard]] std::string& getName() const override {
    static std::string name = "Compute";
    return name;
  }
  void execute(FrameInfo& frameInfo) override;

 private:
  EngineState* engineState_ = nullptr;
};

}  // namespace engine