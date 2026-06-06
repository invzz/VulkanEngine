#pragma once

#include "Engine/Application/Ports/IDescriptorAccessPort.hpp"
#include "Engine/Application/Ports/IRuntimeStatePort.hpp"
#include "Engine/Application/StateViews/RenderingStateView.hpp"
#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class Renderer;
    class Device;

    class OffscreenPass : public IRenderPass {
       public:
        OffscreenPass(Renderer& renderer, RenderingStateView rendering, IDescriptorAccessPort& descriptorAccess, IRuntimeStatePort& runtimeState, Device& device, int& debugMode)
            : renderer_(renderer), rendering_(rendering), descriptorAccess_(descriptorAccess), runtimeState_(runtimeState), device_(device), debugMode_(debugMode) {}

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override {
            static std::string name = "Offscreen";
            return name;
        }

       private:
        void refreshGbufferDescriptors(int frameIndex);

        Renderer&              renderer_;
        RenderingStateView     rendering_;
        IDescriptorAccessPort& descriptorAccess_;
        IRuntimeStatePort&     runtimeState_;
        Device&                device_;
        int&                   debugMode_;
    };

}  // namespace engine
