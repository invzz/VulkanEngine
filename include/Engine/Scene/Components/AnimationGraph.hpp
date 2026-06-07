#ifndef VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONGRAPH_HPP
#define VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONGRAPH_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

#include "AnimationClip.hpp"

namespace engine {

struct AnimationComponent;

/**
 * @brief A node in the animation graph — represents a clip or state
 */
struct AnimationGraphNode {
    int id{-1};
    std::string name;
    int clipIndex{-1};       // -1 means entry/exit node
    std::string clipName;    // For display
    bool isEntry{false};
    bool isExit{false};
    bool isBlendNode{false}; // Interpolation node between clips

    // Runtime state
    float nodeTime{0.0f};
    bool active{false};
    float weight{1.0f};

    AnimationGraphNode() = default;
    AnimationGraphNode(int id, const std::string& name, int clipIdx)
        : id(id), name(name), clipIndex(clipIdx) {}
};

/**
 * @brief Condition that triggers a transition
 */
enum class TransitionCondition {
    NONE,          // Always fire
    TIME_BASED,    // Fire after time elapsed in source node
    EVENT_BASED,   // Fire on animation event
    PARAM_BASED,   // Fire when parameter meets threshold
    BLEND_COMPLETE // Fire when blend finishes
};

/**
 * @brief A transition between two graph nodes
 */
struct AnimationTransition {
    int id{-1};
    std::string name;
    int sourceNodeId{-1};
    int targetNodeId{-1};
    TransitionCondition condition{TransitionCondition::NONE};

    // Condition parameters
    float timeThreshold{0.0f};
    std::string eventName;
    float paramValue{0.0f};
    std::string paramName;

    // Transition behavior
    float blendDuration{0.0f};
    std::string blendMode; // "fade", "crossfade", "instant"
    bool isDefault{false}; // Auto-transition on exit

    AnimationTransition() = default;
    AnimationTransition(int id, const std::string& name, int src, int dst)
        : id(id), name(name), sourceNodeId(src), targetNodeId(dst) {}
};

/**
 * @brief Animation graph — directed state machine
 *
 * Manages a set of nodes and transitions, evaluates conditions,
 * and drives clip transitions with blending.
 */
class AnimationGraph {
public:
    using ConditionEvalFunc = std::function<bool(const AnimationGraph&)>;

    struct Entry {
        int nodeId{-1};
        float startTime{0.0f};
    };

    AnimationGraph() = default;

    /** Add a node to the graph */
    int addNode(const std::string& name, int clipIndex = -1, bool isEntryNode = false);

    /** Add a transition between nodes */
    void addTransition(int sourceId, int targetId, const std::string& name,
                       TransitionCondition cond,
                       float timeThreshold = 0.0f,
                       const std::string& eventName = "",
                       const std::string& paramName = "",
                       float paramValue = 0.0f,
                       float blendDuration = 0.0f,
                       bool isDefault = false);

    /** Find node by ID */
    const AnimationGraphNode* getNode(int nodeId) const;

    /** Find node by name */
    const AnimationGraphNode* getNodeByName(const std::string& name) const;

    /** Get all transitions from a node */
    std::vector<const AnimationTransition*> getTransitions(int sourceId) const;

    /** Get default transition from a node */
    const AnimationTransition* getDefaultTransition(int sourceId) const;

    /** Get entry node */
    const AnimationGraphNode* getEntryNode() const;

    /** Evaluate all conditions from current node and return next transition */
    const AnimationTransition* evaluateNextTransition() const;

    /** Get current active node */
    const AnimationGraphNode* getCurrentNode() const;

    /** Set current active node */
    void setCurrentNode(int nodeId);

    /** Get all registered event names for this graph */
    std::vector<std::string> getRequiredEvents() const;

    /** Get all nodes (for editor) */
    std::vector<AnimationGraphNode> getAllNodes() const;

    /** Get all transitions (for editor) */
    std::vector<AnimationTransition> getTransitionsCopy() const;

    struct TransitionTrigger {
        int sourceNodeId{-1};
        int targetNodeId{-1};
        float blendDuration{0.0f};
        TransitionCondition condition{TransitionCondition::NONE};
        bool triggered{false};
    };

  /** Step the graph — advance time, evaluate conditions. Returns trigger info. */
    TransitionTrigger step(float deltaTime);

private:
    std::vector<AnimationGraphNode> nodes_;
    std::vector<AnimationTransition> transitions_;
    std::unordered_map<int, const AnimationGraphNode*> nodeIndex_;
    std::unordered_map<std::string, int> nodeByName_;
    int currentNodeId_{-1};
    int entryNodeId_{-1};
    float elapsedSinceEntry_{0.0f};

    /** Find transition by source and condition match */
    const AnimationTransition* findTransitionByCondition(int sourceId,
                                                          TransitionCondition cond) const;
};

}  // namespace engine

#endif  // VULKANENGINE_INCLUDE_ENGINE_SCENE_COMPONENTS_ANIMATIONGRAPH_HPP
