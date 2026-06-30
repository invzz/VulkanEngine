#pragma once

#include <functional>

#include "Engine/Graphics/FrameGraph/RenderGraph.hpp"

namespace engine {

    class Renderer;
    class Window;

    class CompositionPass : public IRenderPass {
       public:
        using UIRenderFn = std::function<void(FrameInfo&, VkCommandBuffer, bool)>;

        CompositionPass(Renderer& renderer, UIRenderFn renderUI, Window& window);

        void                             execute(FrameInfo& frameInfo) override;
        [[nodiscard]] const std::string& getName() const override {
            static std::string name = "Composition";
            return name;
        }

       private:
        Renderer&  renderer_;
        UIRenderFn renderUI_;
        Window&    window_;
    };

}  // namespace engine
