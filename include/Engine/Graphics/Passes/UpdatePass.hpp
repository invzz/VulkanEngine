#include "Engine/Application/Ports/IPhysicsRuntimePort.hpp"
#include "Engine/EngineFacade.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class Renderer;
    class FrameInfo;

    class UpdatePass : public IRenderPass {
       public:
        UpdatePass(EngineFacade& engine, IPhysicsRuntimePort* physicsPort, Renderer& renderer)
            : engine_(engine), physicsPort_(physicsPort), renderer(renderer) {}

        [[nodiscard]] const std::string& getName() const override {
            static const std::string name = "Update";
            return name;
        }
        void execute(FrameInfo& frameInfo) override;

       private:
        EngineFacade&        engine_;
        IPhysicsRuntimePort* physicsPort_ = nullptr;
        Renderer&            renderer;
    };

}  // namespace engine