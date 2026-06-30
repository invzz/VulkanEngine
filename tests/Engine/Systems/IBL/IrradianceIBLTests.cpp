#include <gtest/gtest.h>

#include "Engine/Systems/IBL/IBLSettings.hpp"
#include "Engine/Systems/IBL/IrradianceIBL.hpp"

#include "../../../fixtures/DeviceFixture.hpp"

namespace engine::test {

    // Test fixture using shared Device
    class IrradianceIBLTest : public DeviceFixture {};

    // ==============================================================================
    // Construction Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, Construction_DoesNotThrow) {
        EXPECT_NO_THROW({ ibl::IrradianceIBL irradiance(device()); });
    }

    TEST_F(IrradianceIBLTest, Construction_InitializesToNullHandles) {
        ibl::IrradianceIBL irradiance(device());

        EXPECT_EQ(irradiance.image(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.sampler(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Fallback Creation Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, CreateFallback_DoesNotThrow) {
        ibl::IrradianceIBL irradiance(device());
        EXPECT_NO_THROW(irradiance.createFallback());
    }

    TEST_F(IrradianceIBLTest, CreateFallback_CreatesValidImage) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NE(irradiance.image(), VK_NULL_HANDLE);
    }

    TEST_F(IrradianceIBLTest, CreateFallback_CreatesValidImageView) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NE(irradiance.imageView(), VK_NULL_HANDLE);
    }

    TEST_F(IrradianceIBLTest, CreateFallback_CreatesValidSampler) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NE(irradiance.sampler(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Descriptor Info Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, GetDescriptorInfo_AfterFallback_ReturnsValidInfo) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        VkDescriptorImageInfo info = irradiance.getDescriptorInfo();

        EXPECT_NE(info.sampler, VK_NULL_HANDLE);
        EXPECT_NE(info.imageView, VK_NULL_HANDLE);
        EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    TEST_F(IrradianceIBLTest, GetDescriptorInfo_ImageViewMatchesAccessor) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        VkDescriptorImageInfo info = irradiance.getDescriptorInfo();

        EXPECT_EQ(info.imageView, irradiance.imageView());
        EXPECT_EQ(info.sampler, irradiance.sampler());
    }

    // ==============================================================================
    // Reset Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, ResetToUninitialized_ClearsResources) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NE(irradiance.image(), VK_NULL_HANDLE);

        irradiance.resetToUninitialized();

        EXPECT_EQ(irradiance.image(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.sampler(), VK_NULL_HANDLE);
    }

    TEST_F(IrradianceIBLTest, ResetToUninitialized_CanRecreateAfterReset) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();
        irradiance.resetToUninitialized();

        EXPECT_NO_THROW(irradiance.createFallback());
        EXPECT_NE(irradiance.image(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Deferred Destroy Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, DeferDestroyImageResources_DoesNotThrow) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NO_THROW(irradiance.deferDestroyImageResources());
    }

    TEST_F(IrradianceIBLTest, DeferDestroyImageResources_ClearsHandles) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        irradiance.deferDestroyImageResources();

        EXPECT_EQ(irradiance.image(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.sampler(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Immediate Destroy Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, DestroyImmediate_DoesNotThrow) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        EXPECT_NO_THROW(irradiance.destroyImmediate());
    }

    TEST_F(IrradianceIBLTest, DestroyImmediate_OnUninitialized_DoesNotThrow) {
        ibl::IrradianceIBL irradiance(device());

        EXPECT_NO_THROW(irradiance.destroyImmediate());
    }

    TEST_F(IrradianceIBLTest, DestroyImmediate_ClearsAllHandles) {
        ibl::IrradianceIBL irradiance(device());
        irradiance.createFallback();

        irradiance.destroyImmediate();

        EXPECT_EQ(irradiance.image(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(irradiance.sampler(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // CreateForSettings Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, CreateForSettings_SmallSize_DoesNotThrow) {
        ibl::IrradianceIBL irradiance(device());

        ibl::Settings settings;
        settings.irradianceSize = 16;  // Small for test

        EXPECT_NO_THROW(irradiance.createForSettings(settings));
    }

    TEST_F(IrradianceIBLTest, CreateForSettings_CreatesValidResources) {
        ibl::IrradianceIBL irradiance(device());

        ibl::Settings settings;
        settings.irradianceSize = 16;

        irradiance.createForSettings(settings);

        EXPECT_NE(irradiance.image(), VK_NULL_HANDLE);
        EXPECT_NE(irradiance.imageView(), VK_NULL_HANDLE);
        EXPECT_NE(irradiance.sampler(), VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Destructor Tests
    // ==============================================================================

    TEST_F(IrradianceIBLTest, Destructor_CleansUpResources) {
        {
            ibl::IrradianceIBL irradiance(device());
            irradiance.createFallback();
        }

        SUCCEED();
    }

    TEST_F(IrradianceIBLTest, Destructor_WithUninitialized_DoesNotCrash) {
        {
            ibl::IrradianceIBL irradiance(device());
        }

        SUCCEED();
    }

}  // namespace engine::test
