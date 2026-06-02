#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"
#include "Engine/Application/StateViews/InputStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

class Renderer;
class FrameInfo;

class UpdatePass : public IRenderPass {
 public:
  UpdatePass(InputStateView inputState, IPhysicsRuntimePort* physicsPort, Renderer& renderer)
      : inputState_(inputState), physicsPort_(physicsPort), renderer(renderer) {}

  [[nodiscard]] const std::string& getName() const override {
    static const std::string name = "Update";
    return name;
  }
  void execute(FrameInfo& frameInfo) override;

 private:
  InputStateView inputState_;
  IPhysicsRuntimePort* physicsPort_ = nullptr;
  Renderer& renderer;
};

}  // namespace engine