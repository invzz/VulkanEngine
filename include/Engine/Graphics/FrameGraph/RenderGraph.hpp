#ifndef VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERGRAPH_HPP
#define VULKANENGINE_INCLUDE_ENGINE_GRAPHICS_RENDERGRAPH_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Graphics/FrameInfo.hpp"

namespace engine {

    class IRenderPass {
       public:
        virtual ~IRenderPass() = default;

        virtual void                             execute(FrameInfo& frameInfo) = 0;
        [[nodiscard]] virtual const std::string& getName() const               = 0;
    };

    class RenderPassBase : public IRenderPass {
       public:
        explicit RenderPassBase(std::string name) : name_(std::move(name)) {}

        [[nodiscard]] const std::string& getName() const override {
            return name_;
        }

       private:
        std::string name_;
    };

    class LambdaRenderPass : public IRenderPass {
       public:
        using Callback = std::function<void(FrameInfo&)>;

        LambdaRenderPass(std::string name, Callback callback) : name(std::move(name)), callback(std::move(callback)) {}

        void execute(FrameInfo& frameInfo) override {
            if (callback)
                callback(frameInfo);
        }

        [[nodiscard]] const std::string& getName() const override {
            return name;
        }

       private:
        Callback    callback;
        std::string name;
    };

    class RenderGraph {
       public:
        void addPass(std::unique_ptr<IRenderPass> pass);
        void execute(FrameInfo& frameInfo);
        void reset();

       private:
        std::vector<std::unique_ptr<IRenderPass>> passes;
    };

}  // namespace engine

#endif
