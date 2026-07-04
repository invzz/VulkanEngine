#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "Engine/SystemRegistry.hpp"
using namespace engine;
TEST(SystemRegistry, GivenDependencyChainWhenInitializedThenOrderIsDeterministic) {
    SystemRegistry           registry;
    std::vector<std::string> runtimeOrder;
    ASSERT_TRUE(registry.registerSystem("resources", {}, [&runtimeOrder](std::string&) {
        runtimeOrder.push_back("resources");
        return true;
    }));
    ASSERT_TRUE(registry.registerSystem("rendering", {"resources"}, [&runtimeOrder](std::string&) {
        runtimeOrder.push_back("rendering");
        return true;
    }));
    ASSERT_TRUE(registry.registerSystem("ui", {"rendering"}, [&runtimeOrder](std::string&) {
        runtimeOrder.push_back("ui");
        return true;
    }));
    std::string error;
    ASSERT_TRUE(registry.initializeAll(&error)) << error;
    std::vector<std::string> const expected{"resources", "rendering", "ui"};
    EXPECT_EQ(runtimeOrder, expected);
    EXPECT_EQ(registry.initializationOrder(), expected);
}
TEST(SystemRegistry, GivenMissingDependencyWhenInitializedThenReturnsError) {
    SystemRegistry registry;
    ASSERT_TRUE(registry.registerSystem("rendering", {"resources"}, [](std::string&) { return true; }));
    std::string error;
    EXPECT_FALSE(registry.initializeAll(&error));
    EXPECT_FALSE(error.empty());
}
TEST(SystemRegistry, GivenCycleWhenInitializedThenReturnsError) {
    SystemRegistry registry;
    ASSERT_TRUE(registry.registerSystem("A", {"B"}, [](std::string&) { return true; }));
    ASSERT_TRUE(registry.registerSystem("B", {"A"}, [](std::string&) { return true; }));
    std::string error;
    EXPECT_FALSE(registry.initializeAll(&error));
    EXPECT_FALSE(error.empty());
}