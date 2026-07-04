#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCOMPONENT_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONCOMPONENT_HPP
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Engine/Scene/Components/AnimationController.hpp"
#include "Engine/Scene/Components/AnimationGraph.hpp"

#include "ModelLib/Resources/Model.hpp"
namespace engine {
    /**
     * @brief Animation component for entt ECS
     *
     * Holds a shared AnimationController that manages multiple simultaneous clips.
     * The old single-clip fields (currentAnimationIndex, currentTime, etc.) are
     * kept as thin wrappers over the controller for backward compatibility.
     */
    struct AnimationComponent {
        std::shared_ptr<Model>               model;
        std::shared_ptr<AnimationController> controller;
        std::shared_ptr<AnimationGraph>      graph;
        int                                  currentAnimationIndex{-1};
        float                                currentTime{0.0f};
        float                                playbackSpeed{1.0f};
        bool                                 isPlaying{false};
        bool                                 loop{true};
        std::vector<glm::mat4>               nodeTransforms;
        AnimationComponent(std::shared_ptr<Model> m = nullptr) : model(m) {
            if (model) {
                nodeTransforms.resize(model->getNodes().size(), glm::mat4(1.0f));
            }
        }
        /** Play an animation clip (stops all others, starts fresh) */
        void play(int animationIndex = 0, bool shouldLoop = true) {
            if (!model || !controller)
                return;
            if (animationIndex < 0 || animationIndex >= static_cast<int>(model->getAnimations().size()))
                return;
            currentAnimationIndex = animationIndex;
            currentTime           = 0.0f;
            isPlaying             = true;
            loop                  = shouldLoop;
            controller->play(animationIndex, *model);
        }
        /** Stop all playing clips */
        void stop() {
            if (!controller)
                return;
            isPlaying   = false;
            currentTime = 0.0f;
            controller->stopAll();
        }
        /** Add a clip without stopping others (layered) */
        void addClip(int animationIndex, int priority = 0) {
            if (!model || !controller)
                return;
            if (animationIndex < 0 || animationIndex >= static_cast<int>(model->getAnimations().size()))
                return;
            controller->addClip(animationIndex, *model, priority, true);
        }
        /** Set blend weight for a specific clip (0-1) */
        void setClipWeight(int animationIndex, float weight) {
            if (!controller)
                return;
            controller->setClipWeight(animationIndex, weight);
        }
        /** Set playback speed for a specific clip */
        void setClipSpeed(int animationIndex, float speed) {
            if (!controller)
                return;
            controller->setClipSpeed(animationIndex, speed);
        }
        /** Get active clips for UI display */
        const std::vector<AnimationClip>& getActiveClips() const {
            return controller ? controller->getClips() : emptyClips_;
        }
        /** Set an event callback for this component's clips */
        void setEventCallback(std::function<void(const std::string&, void*)> cb) {
            if (!controller)
                return;
            controller->setEventCallback(std::move(cb));
        }
        /** Drain fired events from the controller */
        std::vector<std::pair<std::string, void*>> takeEvents() {
            return controller ? controller->takeEvents() : std::vector<std::pair<std::string, void*>>{};
        }
        /** Set the animation graph for state transitions */
        void setGraph(std::shared_ptr<AnimationGraph> g) {
            graph = g;
            if (controller) {
                controller->setGraph(g);
            }
            if (g && g->getEntryNode()) {
                currentAnimationIndex = g->getEntryNode()->clipIndex;
                currentTime           = 0.0f;
            }
        }
        /** Trigger a transition in the animation graph */
        bool triggerGraphTransition(int targetNodeId) {
            if (!graph)
                return false;
            if (controller) {
                controller->triggerTransition(targetNodeId);
            }
            return true;
        }
        /** Get current graph node name (for display/debug) */
        std::string getCurrentGraphNodeName() const {
            if (controller) {
                return controller->getCurrentGraphNodeName();
            }
            return "None";
        }
        /** Check if a transition is currently in progress */
        bool isTransitioning() const {
            return controller ? controller->isTransitioning() : false;
        }

       private:
        static std::vector<AnimationClip> emptyClips_;
    };
}  // namespace engine
#endif
