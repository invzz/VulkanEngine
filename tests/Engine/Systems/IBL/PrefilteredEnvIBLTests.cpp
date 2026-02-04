#include <gtest/gtest.h>

#include "../../../fixtures/DeviceFixture.hpp"
#include "Engine/Systems/IBL/IBLSettings.hpp"
#include "Engine/Systems/IBL/PrefilteredEnvIBL.hpp"


namespace engine::test {

  // Test fixture using shared Device
  class PrefilteredEnvIBLTest : public DeviceFixture
  {};

  // ==============================================================================
  // Construction Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, Construction_DoesNotThrow)
  {
    EXPECT_NO_THROW({ ibl::PrefilteredEnvIBL prefiltered(device()); });
  }

  TEST_F(PrefilteredEnvIBLTest, Construction_InitializesToNullHandles)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());

    EXPECT_EQ(prefiltered.image(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.imageView(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // Fallback Creation Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, CreateFallback_DoesNotThrow)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    EXPECT_NO_THROW(prefiltered.createFallback());
  }

  TEST_F(PrefilteredEnvIBLTest, CreateFallback_CreatesValidImage)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NE(prefiltered.image(), VK_NULL_HANDLE);
  }

  TEST_F(PrefilteredEnvIBLTest, CreateFallback_CreatesValidImageView)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NE(prefiltered.imageView(), VK_NULL_HANDLE);
  }

  TEST_F(PrefilteredEnvIBLTest, CreateFallback_CreatesValidSampler)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NE(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // Descriptor Info Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, GetDescriptorInfo_AfterFallback_ReturnsValidInfo)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    VkDescriptorImageInfo info = prefiltered.getDescriptorInfo();

    EXPECT_NE(info.sampler, VK_NULL_HANDLE);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
    EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  TEST_F(PrefilteredEnvIBLTest, GetDescriptorInfo_ImageViewMatchesAccessor)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    VkDescriptorImageInfo info = prefiltered.getDescriptorInfo();

    EXPECT_EQ(info.imageView, prefiltered.imageView());
    EXPECT_EQ(info.sampler, prefiltered.sampler());
  }

  // ==============================================================================
  // Reset Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, ResetToUninitialized_ClearsResources)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NE(prefiltered.image(), VK_NULL_HANDLE);

    prefiltered.resetToUninitialized();

    EXPECT_EQ(prefiltered.image(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.imageView(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  TEST_F(PrefilteredEnvIBLTest, ResetToUninitialized_CanRecreateAfterReset)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();
    prefiltered.resetToUninitialized();

    EXPECT_NO_THROW(prefiltered.createFallback());
    EXPECT_NE(prefiltered.image(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // Deferred Destroy Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, DeferDestroyImageResources_DoesNotThrow)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NO_THROW(prefiltered.deferDestroyImageResources());
  }

  TEST_F(PrefilteredEnvIBLTest, DeferDestroyImageResources_ClearsHandles)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    prefiltered.deferDestroyImageResources();

    EXPECT_EQ(prefiltered.image(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.imageView(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // Immediate Destroy Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, DestroyImmediate_DoesNotThrow)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    EXPECT_NO_THROW(prefiltered.destroyImmediate());
  }

  TEST_F(PrefilteredEnvIBLTest, DestroyImmediate_OnUninitialized_DoesNotThrow)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());

    EXPECT_NO_THROW(prefiltered.destroyImmediate());
  }

  TEST_F(PrefilteredEnvIBLTest, DestroyImmediate_ClearsAllHandles)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());
    prefiltered.createFallback();

    prefiltered.destroyImmediate();

    EXPECT_EQ(prefiltered.image(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.imageView(), VK_NULL_HANDLE);
    EXPECT_EQ(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // CreateForSettings Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, CreateForSettings_SmallSize_DoesNotThrow)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());

    ibl::Settings settings;
    settings.prefilterSize      = 32; // Small for test
    settings.prefilterMipLevels = 4;

    EXPECT_NO_THROW(prefiltered.createForSettings(settings));
  }

  TEST_F(PrefilteredEnvIBLTest, CreateForSettings_CreatesValidResources)
  {
    ibl::PrefilteredEnvIBL prefiltered(device());

    ibl::Settings settings;
    settings.prefilterSize      = 32;
    settings.prefilterMipLevels = 4;

    prefiltered.createForSettings(settings);

    EXPECT_NE(prefiltered.image(), VK_NULL_HANDLE);
    EXPECT_NE(prefiltered.imageView(), VK_NULL_HANDLE);
    EXPECT_NE(prefiltered.sampler(), VK_NULL_HANDLE);
  }

  // ==============================================================================
  // Destructor Tests
  // ==============================================================================

  TEST_F(PrefilteredEnvIBLTest, Destructor_CleansUpResources)
  {
    {
      ibl::PrefilteredEnvIBL prefiltered(device());
      prefiltered.createFallback();
    }

    SUCCEED();
  }

  TEST_F(PrefilteredEnvIBLTest, Destructor_WithUninitialized_DoesNotCrash)
  {
    {
      ibl::PrefilteredEnvIBL prefiltered(device());
    }

    SUCCEED();
  }

} // namespace engine::test
