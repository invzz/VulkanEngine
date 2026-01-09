#include <gtest/gtest.h>

#include "xatlas.h"

TEST(UVUnwrapXAtlas, CreateDestroy)
{
  xatlas::Atlas* atlas = xatlas::Create();
  ASSERT_NE(atlas, nullptr);
  xatlas::Destroy(atlas);
}
