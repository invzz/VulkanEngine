#include <gtest/gtest.h>

#include "Engine/Graphics/Instance.hpp"

using namespace engine;

// =============================================================================
// Instance Tests
// =============================================================================

TEST(Instance, GivenDefaultInstance_WhenCreated_ThenIsInvalidHandle) {
  Instance inst;
  EXPECT_FALSE(inst);
}

TEST(Instance, GivenDefaultInstance_WhenCreateCalled_ThenBecomesValid) {
  Instance inst;
  EXPECT_NO_THROW(inst.create({}, {"VK_LAYER_KHRONOS_validation"}));
  EXPECT_TRUE(inst);
}

TEST(Instance, GivenValidInstance_WhenResetCalled_ThenBecomesInvalid) {
  Instance inst;
  inst.create({}, {"VK_LAYER_KHRONOS_validation"});
  EXPECT_TRUE(inst);

  inst.reset();
  EXPECT_FALSE(inst);
}
