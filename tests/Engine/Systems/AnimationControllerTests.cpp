#include <gtest/gtest.h>

#include <memory>

#include "Engine/Scene/Components/AnimationClip.hpp"
#include "Engine/Scene/Components/AnimationController.hpp"

namespace engine {

// ── AnimationClip unit tests ───────────────────────────────────────

TEST(AnimationClipTest, DefaultConstruction) {
    AnimationClip clip;
    EXPECT_EQ(clip.clipIndex, -1);
    EXPECT_EQ(clip.duration, 0.0f);
    EXPECT_EQ(clip.currentTime, 0.0f);
    EXPECT_EQ(clip.speed, 1.0f);
    EXPECT_EQ(clip.weight, 1.0f);
    EXPECT_TRUE(clip.loop);
    EXPECT_FALSE(clip.active);
    EXPECT_EQ(clip.mode, AnimationClip::OVERRIDE);
    EXPECT_EQ(clip.priority, 0);
}

TEST(AnimationClipTest, Reset) {
    AnimationClip clip;
    clip.active = true;
    clip.weight = 0.5f;
    clip.currentTime = 10.0f;
    clip.nextEventIndex = 5;

    clip.reset();

    EXPECT_FALSE(clip.active);
    EXPECT_EQ(clip.weight, 1.0f);
    EXPECT_EQ(clip.currentTime, 0.0f);
    EXPECT_EQ(clip.nextEventIndex, 0);
}

TEST(AnimationClipTest, IsFinished) {
    AnimationClip clip;
    clip.duration = 5.0f;
    clip.loop = false;

    // Before playing
    EXPECT_FALSE(clip.isFinished());

    // Playing but not finished
    clip.active = true;
    clip.currentTime = 3.0f;
    EXPECT_FALSE(clip.isFinished());

    // Finished (past duration)
    clip.currentTime = 6.0f;
    EXPECT_TRUE(clip.isFinished());

    // Looping — never finishes
    clip.loop = true;
    clip.currentTime = 10.0f;
    EXPECT_FALSE(clip.isFinished());
}

TEST(AnimationClipTest, HasEvents) {
    AnimationClip clip;
    EXPECT_FALSE(clip.hasEvents());

    clip.events.push_back({1.0f, "event1", nullptr});
    // hasEvents() checks if events exist and haven't all been drained
    // It does NOT check time readiness — that's drainFiredEvents' job
    EXPECT_TRUE(clip.hasEvents());  // has events, nextEventIndex=0 < 1

    // Drain one
    clip.currentTime = 2.0f;
    std::vector<std::pair<std::string, void*>> fired;
    clip.drainFiredEvents(fired);
    // Still has events (next is at time 5.0, but hasEvents doesn't check time)
    EXPECT_FALSE(clip.hasEvents());  // nextEventIndex=1 == events.size()

    // Add more events
    clip.events.push_back({3.0f, "event3", nullptr});
    EXPECT_TRUE(clip.hasEvents());

    clip.nextEventIndex = 2;
    EXPECT_FALSE(clip.hasEvents());  // drained
}

TEST(AnimationClipTest, DrainFiredEvents) {
    AnimationClip clip;
    clip.currentTime = 3.0f;

    clip.events.push_back({1.0f, "first", (void*)0x1});
    clip.events.push_back({2.0f, "second", (void*)0x2});
    clip.events.push_back({5.0f, "future", (void*)0x3});

    std::vector<std::pair<std::string, void*>> fired;
    clip.drainFiredEvents(fired);

    EXPECT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0].first, "first");
    EXPECT_EQ(fired[0].second, (void*)0x1);
    EXPECT_EQ(fired[1].first, "second");
    EXPECT_EQ(fired[1].second, (void*)0x2);
    EXPECT_EQ(clip.nextEventIndex, 2u);
}

// ── AnimationController unit tests ─────────────────────────────────

TEST(AnimationControllerTest, DefaultState) {
    AnimationController controller;
    EXPECT_FALSE(controller.hasActiveClips());
    EXPECT_TRUE(controller.getClips().empty());
}

TEST(AnimationControllerTest, HasActiveClipsDetectsActive) {
    AnimationController controller;

    // No clips = inactive
    EXPECT_FALSE(controller.hasActiveClips());

    // Add a non-active clip
    AnimationClip clip;
    clip.clipIndex = 0;
    controller.getClips().push_back(std::move(clip));
    EXPECT_FALSE(controller.hasActiveClips());

    // Activate it
    controller.getClips()[0].active = true;
    controller.getClips()[0].weight = 1.0f;
    EXPECT_TRUE(controller.hasActiveClips());

    // Set weight to 0 = inactive
    controller.getClips()[0].weight = 0.0f;
    EXPECT_FALSE(controller.hasActiveClips());
}

TEST(AnimationControllerTest, GetClipReturnsNullWhenNotFound) {
    AnimationController controller;
    EXPECT_EQ(controller.getClip(0), nullptr);

    // Add a clip
    AnimationClip c;
    c.clipIndex = 5;
    controller.getClips().push_back(std::move(c));

    // Get by wrong index
    EXPECT_EQ(controller.getClip(0), nullptr);

    // Get by correct index
    auto* found = controller.getClip(5);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->clipIndex, 5);
}

TEST(AnimationControllerTest, SetClipWeightClamps) {
    AnimationController controller;

    AnimationClip clip;
    clip.clipIndex = 0;
    clip.weight = 1.0f;
    controller.getClips().push_back(std::move(clip));

    controller.setClipWeight(0, 0.5f);
    EXPECT_FLOAT_EQ(controller.getClips()[0].weight, 0.5f);

    // Clamp to 0
    controller.setClipWeight(0, -1.0f);
    EXPECT_FLOAT_EQ(controller.getClips()[0].weight, 0.0f);

    // Clamp to 1
    controller.setClipWeight(0, 2.0f);
    EXPECT_FLOAT_EQ(controller.getClips()[0].weight, 1.0f);
}

TEST(AnimationControllerTest, SetClipSpeedClamps) {
    AnimationController controller;

    AnimationClip clip;
    clip.clipIndex = 0;
    clip.speed = 1.0f;
    controller.getClips().push_back(std::move(clip));

    controller.setClipSpeed(0, 2.0f);
    EXPECT_FLOAT_EQ(controller.getClips()[0].speed, 2.0f);

    // Clamp minimum
    controller.setClipSpeed(0, -5.0f);
    EXPECT_FLOAT_EQ(controller.getClips()[0].speed, 0.01f);
}

TEST(AnimationControllerTest, StopClip) {
    AnimationController controller;

    AnimationClip clip;
    clip.clipIndex = 0;
    clip.active = true;
    clip.weight = 1.0f;
    clip.currentTime = 2.5f;
    clip.nextEventIndex = 1;
    controller.getClips().push_back(std::move(clip));

    controller.stop(0);

    auto* found = controller.getClip(0);
    ASSERT_NE(found, nullptr);
    EXPECT_FALSE(found->active);
    EXPECT_FLOAT_EQ(found->weight, 0.0f);
    EXPECT_EQ(found->currentTime, 0.0f);
    EXPECT_EQ(found->nextEventIndex, 0u);
}

TEST(AnimationControllerTest, StopAll) {
    AnimationController controller;

    AnimationClip c1;
    c1.clipIndex = 0;
    c1.active = true;
    c1.weight = 1.0f;
    controller.getClips().push_back(std::move(c1));

    AnimationClip c2;
    c2.clipIndex = 1;
    c2.active = true;
    c2.weight = 0.5f;
    controller.getClips().push_back(std::move(c2));

    controller.stopAll();

    EXPECT_FALSE(controller.hasActiveClips());
    for (const auto& clip : controller.getClips()) {
        EXPECT_FALSE(clip.active);
        EXPECT_FLOAT_EQ(clip.weight, 0.0f);
        EXPECT_EQ(clip.currentTime, 0.0f);
        EXPECT_EQ(clip.nextEventIndex, 0u);
    }
}

TEST(AnimationControllerTest, Reset) {
    AnimationController controller;

    AnimationClip clip;
    clip.clipIndex = 0;
    clip.active = true;
    clip.weight = 0.3f;
    clip.currentTime = 5.0f;
    controller.getClips().push_back(std::move(clip));

    controller.reset();

    auto* found = controller.getClip(0);
    ASSERT_NE(found, nullptr);
    EXPECT_FALSE(found->active);
    EXPECT_EQ(found->weight, 1.0f);
    EXPECT_EQ(found->currentTime, 0.0f);
    EXPECT_EQ(found->nextEventIndex, 0u);
}

TEST(AnimationControllerTest, RemoveClip) {
    AnimationController controller;

    AnimationClip c1;
    c1.clipIndex = 0;
    c1.name = "Clip0";
    c1.active = true;
    controller.getClips().push_back(std::move(c1));

    AnimationClip c2;
    c2.clipIndex = 1;
    c2.name = "Clip1";
    c2.active = false;
    controller.getClips().push_back(std::move(c2));

    controller.removeClip(0);

    EXPECT_EQ(controller.getClips().size(), 1u);
    EXPECT_EQ(controller.getClips()[0].clipIndex, 1);

    // Removing non-existent clip is safe
    controller.removeClip(99);
    EXPECT_EQ(controller.getClips().size(), 1u);
}

}  // namespace engine
