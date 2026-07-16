#include "Engine/Scene/Animation/AnimationGraph.hpp"

#include <algorithm>

#include "Engine/Scene/components/AnimationComponent.hpp"
namespace engine {
    int AnimationGraph::addNode(const std::string& name, int clipIndex, bool isEntryNode) {
        int                id = static_cast<int>(nodes_.size());
        AnimationGraphNode node(id, name, clipIndex);
        node.isEntry = isEntryNode;
        nodes_.push_back(node);
        nodeIndex_[id]    = &nodes_.back();
        nodeByName_[name] = id;
        if (isEntryNode) {
            entryNodeId_   = id;
            currentNodeId_ = id;
        }
        return id;
    }
    void AnimationGraph::addTransition(int sourceId, int targetId, const std::string& name,
        TransitionCondition cond,
        float               timeThreshold,
        const std::string&  eventName,
        const std::string&  paramName,
        float               paramValue,
        float               blendDuration,
        bool                isDefault) {
        int                 id = static_cast<int>(transitions_.size());
        AnimationTransition transition;
        transition.id            = id;
        transition.name          = name;
        transition.sourceNodeId  = sourceId;
        transition.targetNodeId  = targetId;
        transition.condition     = cond;
        transition.timeThreshold = timeThreshold;
        transition.eventName     = eventName;
        transition.paramName     = paramName;
        transition.paramValue    = paramValue;
        transition.blendDuration = blendDuration;
        transition.isDefault     = isDefault;
        transitions_.push_back(transition);
    }
    const AnimationGraphNode* AnimationGraph::getNode(int nodeId) const {
        auto it = nodeIndex_.find(nodeId);
        if (it != nodeIndex_.end())
            return it->second;
        return nullptr;
    }
    const AnimationGraphNode* AnimationGraph::getNodeByName(const std::string& name) const {
        auto it = nodeByName_.find(name);
        if (it != nodeByName_.end())
            return getNode(it->second);
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
    std::vector<AnimationTransition> AnimationGraph::getTransitionsCopy() const {
        return transitions_;
    }
    std::vector<AnimationGraphNode> AnimationGraph::getAllNodes() const {
        return nodes_;
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
        currentNodeId_     = nodeId;
        elapsedSinceEntry_ = 0.0f;
    }
    const AnimationTransition* AnimationGraph::findTransitionByCondition(int sourceId,
        TransitionCondition                                                  cond) const {
        for (const auto& t : transitions_) {
            if (t.sourceNodeId == sourceId && t.condition == cond && !t.isDefault) {
                return &t;
            }
        }
        return nullptr;
    }
    const AnimationTransition* AnimationGraph::evaluateNextTransition() const {
        const auto* current = getNode(currentNodeId_);
        if (!current)
            return nullptr;
        auto trans = getTransitions(current->id);
        for (const auto* t : trans) {
            switch (t->condition) {
                case TransitionCondition::NONE:
                    return t;
                case TransitionCondition::TIME_BASED:
                    break;
                case TransitionCondition::EVENT_BASED:
                    break;
                case TransitionCondition::PARAM_BASED:
                    break;
                case TransitionCondition::BLEND_COMPLETE:
                    break;
            }
        }
        for (const auto* t : trans) {
            if (t->condition == TransitionCondition::TIME_BASED) {
                return t;
            }
        }
        return getDefaultTransition(current->id);
    }
    AnimationGraph::TransitionTrigger AnimationGraph::step(float deltaTime) {
        AnimationGraph::TransitionTrigger trigger;
        trigger.sourceNodeId = currentNodeId_;
        const auto* current  = getNode(currentNodeId_);
        if (!current)
            return trigger;
        elapsedSinceEntry_ += deltaTime;
        auto trans = getTransitions(current->id);
        for (const auto* t : trans) {
            if (t->condition == TransitionCondition::TIME_BASED) {
                if (elapsedSinceEntry_ >= t->timeThreshold) {
                    setCurrentNode(t->targetNodeId);
                    elapsedSinceEntry_    = 0.0f;
                    trigger.targetNodeId  = t->targetNodeId;
                    trigger.blendDuration = t->blendDuration;
                    trigger.condition     = t->condition;
                    trigger.triggered     = true;
                    break;
                }
            }
        }
        if (!trigger.triggered) {
            const auto* defaultTrans = getDefaultTransition(current->id);
            if (defaultTrans) {
                setCurrentNode(defaultTrans->targetNodeId);
                elapsedSinceEntry_    = 0.0f;
                trigger.targetNodeId  = defaultTrans->targetNodeId;
                trigger.blendDuration = defaultTrans->blendDuration;
                trigger.condition     = defaultTrans->condition;
                trigger.triggered     = true;
            }
        }
        if (currentNodeId_ >= 0 && static_cast<size_t>(currentNodeId_) < nodes_.size()) {
            nodes_[static_cast<size_t>(currentNodeId_)].active = true;
        }
        const auto* newCurrent = getCurrentNode();
        if (newCurrent && newCurrent->id >= 0 && static_cast<size_t>(newCurrent->id) < nodes_.size()) {
            nodes_[static_cast<size_t>(newCurrent->id)].active = true;
        }
        return trigger;
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
