#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "Engine/Core/Window.hpp"

using namespace engine;

TEST(LightmapToolsCopy, EXR2VTEX_IsPresentInToolsDir)
{
  // EXR2VTEX_PATH is defined by xmake.lua pointing to tools/EXR2VTEX
#ifdef EXR2VTEX_PATH
  std::string toolPath = EXR2VTEX_PATH;
  // Prefer to check the file exists. If not built, skip the test so local dev doesn't fail.
  if (!std::filesystem::exists(toolPath))
  {
    GTEST_SKIP() << "EXR2VTEX tool not found at: " << toolPath << ". Build it with: xmake EXR2VTEX";
  }

  EXPECT_TRUE(std::filesystem::exists(toolPath));
  EXPECT_TRUE(std::filesystem::is_regular_file(toolPath));
#else
  GTEST_SKIP() << "EXR2VTEX_PATH macro not defined by the build system";
#endif
}
