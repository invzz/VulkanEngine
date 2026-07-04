#include <gtest/gtest.h>

#include "Engine/Graphics/DebugMessenger.hpp"
#include "Engine/Graphics/Instance.hpp"
using namespace engine;
TEST(DebugMessenger, GivenDefaultMessenger_WhenCreated_ThenIsInvalidHandle) {
    DebugMessenger dbg;
    EXPECT_FALSE(dbg);
}
TEST(DebugMessenger, GivenValidInstance_WhenMessengerCreated_ThenBecomesValid) {
    Instance inst;
    inst.create({VK_EXT_DEBUG_UTILS_EXTENSION_NAME}, {"VK_LAYER_KHRONOS_validation"});
    EXPECT_TRUE(inst);
    DebugMessenger dbg;
    EXPECT_NO_THROW(dbg.create(inst.get()));
    EXPECT_TRUE(dbg);
}
TEST(DebugMessenger, GivenValidMessenger_WhenResetCalled_ThenBecomesInvalid) {
    Instance inst;
    inst.create({VK_EXT_DEBUG_UTILS_EXTENSION_NAME}, {"VK_LAYER_KHRONOS_validation"});
    DebugMessenger dbg;
    dbg.create(inst.get());
    EXPECT_TRUE(dbg);
    dbg.reset();
    EXPECT_FALSE(dbg);
}
TEST(DebugMessenger, GivenInvalidMessenger_WhenResetCalledMultipleTimes_ThenNoThrow) {
    DebugMessenger dbg;
    EXPECT_NO_THROW(dbg.reset());
    EXPECT_NO_THROW(dbg.reset());
    EXPECT_FALSE(dbg);
}
