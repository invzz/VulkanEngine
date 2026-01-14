#include <gtest/gtest.h>

#include "Engine/Graphics/DebugMessenger.hpp"
#include "Engine/Graphics/Instance.hpp"

using namespace engine;

TEST(Instance, CreateAndDestroy)
{
  Instance inst;
  EXPECT_FALSE(inst);
  EXPECT_NO_THROW(inst.create({}, {"VK_LAYER_KHRONOS_validation"}));
  EXPECT_TRUE(inst);
  inst.reset();
  EXPECT_FALSE(inst);
}

TEST(DebugMessenger, CreateResetLifecycle)
{
  Instance inst;
  // Create instance with debug utils + validation layers to allow messenger creation
  EXPECT_NO_THROW(inst.create({VK_EXT_DEBUG_UTILS_EXTENSION_NAME}, {"VK_LAYER_KHRONOS_validation"}));
  EXPECT_TRUE(inst);

  DebugMessenger dbg;
  EXPECT_FALSE(dbg);
  EXPECT_NO_THROW(dbg.create(inst.get()));
  EXPECT_TRUE(dbg);
  // Reset should be noexcept and idempotent
  dbg.reset();
  EXPECT_FALSE(dbg);
  dbg.reset();
  EXPECT_FALSE(dbg);

  // Recreate is allowed
  EXPECT_NO_THROW(dbg.create(inst.get()));
  EXPECT_TRUE(dbg);
}
