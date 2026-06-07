#include <gtest/gtest.h>
#include "Engine/Scene/Components/AnimationGraph.hpp"
#include "Engine/Scene/components/AnimationComponent.hpp"

namespace engine {
namespace {

TEST(AnimationGraphTest, DefaultConstruction) {
    AnimationGraph graph;
    ASSERT_EQ(graph.getEntryNode(), nullptr);
    ASSERT_EQ(graph.getCurrentNode(), nullptr);
    ASSERT_EQ(graph.getTransitions(0).size(), 0);
}

TEST(AnimationGraphTest, AddEntryNode) {
    AnimationGraph graph;
    int id = graph.addNode("Entry", -1, true);
    
    ASSERT_EQ(id, 0);
    auto* node = graph.getNode(0);
    ASSERT_NE(node, nullptr);
    ASSERT_EQ(node->name, "Entry");
    ASSERT_EQ(node->clipIndex, -1);
    ASSERT_TRUE(node->isEntry);
    ASSERT_FALSE(node->isExit);
    
    ASSERT_EQ(graph.getEntryNode(), node);
    ASSERT_EQ(graph.getCurrentNode(), node);
}

TEST(AnimationGraphTest, AddMultipleNodes) {
    AnimationGraph graph;
    int entryId = graph.addNode("Entry", -1, true);
    int idleId = graph.addNode("Idle", 0);
    int walkId = graph.addNode("Walk", 1);
    int runId = graph.addNode("Run", 2);
    
    ASSERT_EQ(entryId, 0);
    ASSERT_EQ(idleId, 1);
    ASSERT_EQ(walkId, 2);
    ASSERT_EQ(runId, 3);
    
    ASSERT_NE(graph.getNode(0), nullptr);
    ASSERT_NE(graph.getNode(1), nullptr);
    ASSERT_NE(graph.getNode(2), nullptr);
    ASSERT_NE(graph.getNode(3), nullptr);
    ASSERT_EQ(graph.getNode(99), nullptr);
}

TEST(AnimationGraphTest, AddNodeByName) {
    AnimationGraph graph;
    int id = graph.addNode("MyNode", 5);
    
    ASSERT_NE(graph.getNodeByName("MyNode"), nullptr);
    ASSERT_EQ(graph.getNodeByName("MyNode")->clipIndex, 5);
    ASSERT_EQ(graph.getNodeByName("NonExistent"), nullptr);
}

TEST(AnimationGraphTest, AddTransition) {
    AnimationGraph graph;
    graph.addNode("Entry", -1, true);
    int idleId = graph.addNode("Idle", 0);
    int walkId = graph.addNode("Walk", 1);
    
    graph.addTransition(idleId, walkId, "ToWalk",
                        TransitionCondition::TIME_BASED,
                        2.0f, "", "", 0.0f, 0.5f, false);
    
    auto trans = graph.getTransitions(idleId);
    ASSERT_EQ(trans.size(), 1);
    ASSERT_EQ(trans[0]->name, "ToWalk");
    ASSERT_EQ(trans[0]->targetNodeId, walkId);
    ASSERT_EQ(trans[0]->condition, TransitionCondition::TIME_BASED);
    ASSERT_NEAR(trans[0]->timeThreshold, 2.0f, 0.001f);
    ASSERT_NEAR(trans[0]->blendDuration, 0.5f, 0.001f);
}

TEST(AnimationGraphTest, AddDefaultTransition) {
    AnimationGraph graph;
    graph.addNode("Entry", -1, true);
    int idleId = graph.addNode("Idle", 0);
    int walkId = graph.addNode("Walk", 1);
    
    graph.addTransition(idleId, walkId, "AutoToWalk",
                        TransitionCondition::NONE,
                        0.0f, "", "", 0.0f, 0.0f, true);
    
    auto* defTrans = graph.getDefaultTransition(idleId);
    ASSERT_NE(defTrans, nullptr);
    ASSERT_EQ(defTrans->name, "AutoToWalk");
    ASSERT_TRUE(defTrans->isDefault);
}

TEST(AnimationGraphTest, GetCurrentNode) {
    AnimationGraph graph;
    int idleId = graph.addNode("Entry", -1, true);
    
    ASSERT_EQ(graph.getCurrentNode()->id, idleId);
    
    graph.setCurrentNode(-1);
    ASSERT_EQ(graph.getCurrentNode(), nullptr);
}

TEST(AnimationGraphTest, StepAdvancesTime) {
    AnimationGraph graph;
    graph.addNode("Entry", -1, true);
    int idleId = graph.addNode("Idle", 0);
    int walkId = graph.addNode("Walk", 1);
    
    // Set current node to idle
    graph.setCurrentNode(idleId);
    
    // Before step: elapsed time = 0
    // Add time-based transition with threshold 2.0s
    graph.addTransition(idleId, walkId, "ToWalk",
                        TransitionCondition::TIME_BASED,
                        2.0f);
    
    // Step with 1.0f — should not trigger (below threshold)
    AnimationComponent mockComp;
    graph.step(1.0f, mockComp);
    
    // Step with 2.0f — should trigger
    graph.step(2.0f, mockComp);
    
    // Current node should now be walkId
    ASSERT_EQ(graph.getCurrentNode()->id, walkId);
}

TEST(AnimationGraphTest, GetRequiredEvents) {
    AnimationGraph graph;
    graph.addNode("Entry", -1, true);
    int idleId = graph.addNode("Idle", 0);
    int runId = graph.addNode("Run", 2);
    
    graph.addTransition(idleId, runId, "OnAttack",
                        TransitionCondition::EVENT_BASED,
                        0.0f, "Attack");
    
    auto events = graph.getRequiredEvents();
    ASSERT_EQ(events.size(), 1);
    ASSERT_EQ(events[0], "Attack");
}

TEST(AnimationGraphTest, GetTransitionsFromUnknownNode) {
    AnimationGraph graph;
    auto trans = graph.getTransitions(999);
    ASSERT_TRUE(trans.empty());
}

}  // namespace
}  // namespace engine
