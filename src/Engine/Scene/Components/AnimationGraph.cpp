#include "Engine/Scene/Components/AnimationGraph.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"

#include <algorithm>
#include <cmath>

namespace engine {

    int AnimationGraph::addNode(const std::string& name, int clipIndex, bool isEntryNode) {
        int id = static_cast<int>(nodes_.size());
        AnimationGraphNode node(id, name, clipIndex);
        node.isEntry = isEntryNode;
        nodes_.push_back(node);
        nodeIndex_[id] = &nodes_.back();
        nodeByName_[name] = id;
        if (isEntryNode) {
            entryNodeId_ = id;
            currentNodeId_ = id;
        }
        return id;
    }

    void AnimationGraph::addTransition(int sourceId, int targetId, const std::string& name,
                                        TransitionCondition cond,
                                        float timeThreshold,
                                        const std::string& eventName,
                                        const std::string& paramName,
                                        float paramValue,
                                        float blendDuration,
                                        bool isDefault) {
        int id = static_cast<int>(transitions_.size());
        AnimationTransition transition;
        transition.id = id;
        transition.name = name;
        transition.sourceNodeId = sourceId;
        transition.targetNodeId = targetId;
        transition.condition = cond;
        transition.timeThreshold = timeThreshold;
        transition.eventName = eventName;
        transition.paramName = paramName;
        transition.paramValue = paramValue;
        transition.blendDuration = blendDuration;
        transition.isDefault = isDefault;
        transitions_.push_back(transition);
    }

    const AnimationGraphNode* AnimationGraph::getNode(int nodeId) const {
        auto it = nodeIndex_.find(nodeId);
        if (it != nodeIndex_.end()) return it->second;
        return nullptr;
    }

    const AnimationGraphNode* AnimationGraph::getNodeByName(const std::string& name) const {
        auto it = nodeByName_.find(name);
        if (it != nodeByName_.end()) return getNode(it->second);
        return nullptr;
    }

    std::vector<const AnimationTransition*> AnimationGraph::getTransitions(int sourceId) const {
        std::vector<const AnimationTransition*> result;
        for (const auto& t : transitions_) {
            if (t.sourceNodeId == sourceId) {
                result.push_back(&t);
            }
        }
        return result;
    }

    const AnimationTransition* AnimationGraph::getDefaultTransition(int sourceId) const {
        for (const auto& t : transitions_) {
            if (t.sourceNodeId == sourceId && t.isDefault) {
                return &t;
            }
        }
        return nullptr;
    }

    const AnimationGraphNode* AnimationGraph::getEntryNode() const {
        return getNode(entryNodeId_);
    }

    const AnimationGraphNode* AnimationGraph::getCurrentNode() const {
        return getNode(currentNodeId_);
    }

    void AnimationGraph::setCurrentNode(int nodeId) {
        currentNodeId_ = nodeId;
        elapsedSinceEntry_ = 0.0f;
    }

    const AnimationTransition* AnimationGraph::findTransitionByCondition(int sourceId,
                                                                          TransitionCondition cond) const {
        for (const auto& t : transitions_) {
            if (t.sourceNodeId == sourceId && t.condition == cond && !t.isDefault) {
                return &t;
            }
        }
        return nullptr;
    }

    const AnimationTransition* AnimationGraph::evaluateNextTransition() const {
        const auto* current = getNode(currentNodeId_);
        if (!current) return nullptr;

        auto trans = getTransitions(current->id);

        // Check explicit conditions first (prioritized)
        for (const auto* t : trans) {
            switch (t->condition) {
                case TransitionCondition::NONE:
                    return t;  // Always fires, prefer non-default
                case TransitionCondition::TIME_BASED:
                    // Handled in step() with elapsed time check
                    break;
                case TransitionCondition::EVENT_BASED:
                    // Handled externally via event callback
                    break;
                case TransitionCondition::PARAM_BASED:
                    // Handled externally via parameter check
                    break;
                case TransitionCondition::BLEND_COMPLETE:
                    break;
            }
        }

        // Check time-based transitions
        for (const auto* t : trans) {
            if (t->condition == TransitionCondition::TIME_BASED) {
                // Time check done in step() — return for evaluation
                return t;
            }
        }

        // Default transition (auto on exit)
        return getDefaultTransition(current->id);
    }

    void AnimationGraph::step(float deltaTime, const AnimationComponent& comp) {
        const auto* current = getNode(currentNodeId_);
        if (!current) return;

        elapsedSinceEntry_ += deltaTime;

        // Evaluate time-based transitions
        auto trans = getTransitions(current->id);
        for (const auto* t : trans) {
            if (t->condition == TransitionCondition::TIME_BASED) {
                if (elapsedSinceEntry_ >= t->timeThreshold) {
                    // Trigger transition
                    setCurrentNode(t->targetNodeId);
                    elapsedSinceEntry_ = 0.0f;
                    break;
                }
            }
        }
    }

    std::vector<std::string> AnimationGraph::getRequiredEvents() const {
        std::vector<std::string> events;
        for (const auto& t : transitions_) {
            if (t.condition == TransitionCondition::EVENT_BASED && !t.eventName.empty()) {
                events.push_back(t.eventName);
            }
        }
        return events;
    }

}  // namespace engine
