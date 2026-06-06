#include "Engine/Application/Ports/IAnimationAccessPort.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class FrameInfo;

    class ComputePass : public IRenderPass {
       public:
        explicit ComputePass(IAnimationAccessPort* animationPort)
            : animationPort_(animationPort) {}

        [[nodiscard]] const std::string& getName() const override {
            static std::string name = "Compute";
            return name;
        }
        void execute(FrameInfo& frameInfo) override;

       private:
        IAnimationAccessPort* animationPort_ = nullptr;
    };

}  // namespace engine