#include <gtest/gtest.h>

#include "Engine/Systems/IBL/BRDFLUT.hpp"
#include "Engine/Systems/IBL/IBLSettings.hpp"

#include "../../../fixtures/DeviceFixture.hpp"
namespace engine::test {
    class BRDFLUTTest : public DeviceFixture {};
    TEST_F(BRDFLUTTest, Construction_DoesNotThrow) {
        EXPECT_NO_THROW({ ibl::BRDFLUT brdfLUT(device()); });
    }
    TEST_F(BRDFLUTTest, Construction_InitializesToNullHandles) {
        ibl::BRDFLUT brdfLUT(device());
        EXPECT_EQ(brdfLUT.image(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.sampler(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.currentSize(), 0);
    }
    TEST_F(BRDFLUTTest, CreateFallback_DoesNotThrow) {
        ibl::BRDFLUT brdfLUT(device());
        EXPECT_NO_THROW(brdfLUT.createFallback());
    }
    TEST_F(BRDFLUTTest, CreateFallback_CreatesValidImage) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NE(brdfLUT.image(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, CreateFallback_CreatesValidImageView) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NE(brdfLUT.imageView(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, CreateFallback_CreatesValidSampler) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NE(brdfLUT.sampler(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, CreateFallback_SetsSize1) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_EQ(brdfLUT.currentSize(), 1);
    }
    TEST_F(BRDFLUTTest, GetDescriptorInfo_AfterFallback_ReturnsValidInfo) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        VkDescriptorImageInfo info = brdfLUT.getDescriptorInfo();
        EXPECT_NE(info.sampler, VK_NULL_HANDLE);
        EXPECT_NE(info.imageView, VK_NULL_HANDLE);
        EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    TEST_F(BRDFLUTTest, GetDescriptorInfo_ImageViewMatchesAccessor) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        VkDescriptorImageInfo info = brdfLUT.getDescriptorInfo();
        EXPECT_EQ(info.imageView, brdfLUT.imageView());
        EXPECT_EQ(info.sampler, brdfLUT.sampler());
    }
    TEST_F(BRDFLUTTest, ResetToUninitialized_ClearsResources) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NE(brdfLUT.image(), VK_NULL_HANDLE);
        brdfLUT.resetToUninitialized();
        EXPECT_EQ(brdfLUT.image(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.sampler(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.currentSize(), 0);
    }
    TEST_F(BRDFLUTTest, ResetToUninitialized_CanRecreateAfterReset) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        brdfLUT.resetToUninitialized();
        EXPECT_NO_THROW(brdfLUT.createFallback());
        EXPECT_NE(brdfLUT.image(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, DeferDestroyImageResources_DoesNotThrow) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NO_THROW(brdfLUT.deferDestroyImageResources());
    }
    TEST_F(BRDFLUTTest, DeferDestroyImageResources_ClearsHandles) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        brdfLUT.deferDestroyImageResources();
        EXPECT_EQ(brdfLUT.image(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.sampler(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, DestroyImmediate_DoesNotThrow) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        EXPECT_NO_THROW(brdfLUT.destroyImmediate());
    }
    TEST_F(BRDFLUTTest, DestroyImmediate_OnUninitialized_DoesNotThrow) {
        ibl::BRDFLUT brdfLUT(device());
        EXPECT_NO_THROW(brdfLUT.destroyImmediate());
    }
    TEST_F(BRDFLUTTest, DestroyImmediate_ClearsAllHandles) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        brdfLUT.destroyImmediate();
        EXPECT_EQ(brdfLUT.image(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.imageView(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.sampler(), VK_NULL_HANDLE);
        EXPECT_EQ(brdfLUT.currentSize(), 0);
    }
    TEST_F(BRDFLUTTest, EnsureForSettings_SmallSize_DoesNotThrow) {
        ibl::BRDFLUT  brdfLUT(device());
        ibl::Settings settings;
        settings.brdfLUTSize = 32;
        EXPECT_NO_THROW(brdfLUT.ensureForSettings(settings));
    }
    TEST_F(BRDFLUTTest, EnsureForSettings_CreatesCorrectSize) {
        ibl::BRDFLUT  brdfLUT(device());
        ibl::Settings settings;
        settings.brdfLUTSize = 64;
        brdfLUT.ensureForSettings(settings);
        EXPECT_EQ(brdfLUT.currentSize(), 64);
    }
    TEST_F(BRDFLUTTest, EnsureForSettings_CreatesValidResources) {
        ibl::BRDFLUT  brdfLUT(device());
        ibl::Settings settings;
        settings.brdfLUTSize = 32;
        brdfLUT.ensureForSettings(settings);
        EXPECT_NE(brdfLUT.image(), VK_NULL_HANDLE);
        EXPECT_NE(brdfLUT.imageView(), VK_NULL_HANDLE);
        EXPECT_NE(brdfLUT.sampler(), VK_NULL_HANDLE);
    }
    TEST_F(BRDFLUTTest, EnsureForSettings_SameSize_NoRegeneration) {
        ibl::BRDFLUT  brdfLUT(device());
        ibl::Settings settings;
        settings.brdfLUTSize = 64;
        brdfLUT.ensureForSettings(settings);
        VkImage firstImage = brdfLUT.image();
        brdfLUT.ensureForSettings(settings);
        EXPECT_EQ(brdfLUT.image(), firstImage);
    }
    TEST_F(BRDFLUTTest, EnsureForSettings_DifferentSize_Regenerates) {
        ibl::BRDFLUT  brdfLUT(device());
        ibl::Settings settings1;
        settings1.brdfLUTSize = 32;
        brdfLUT.ensureForSettings(settings1);
        EXPECT_EQ(brdfLUT.currentSize(), 32);
        ibl::Settings settings2;
        settings2.brdfLUTSize = 64;
        brdfLUT.ensureForSettings(settings2);
        EXPECT_EQ(brdfLUT.currentSize(), 64);
    }
    TEST_F(BRDFLUTTest, AdoptLoaded_UpdatesAccessors) {
        ibl::BRDFLUT brdfLUT(device());
        brdfLUT.createFallback();
        VkImage      originalImage     = brdfLUT.image();
        VkImageView  originalImageView = brdfLUT.imageView();
        VkSampler    originalSampler   = brdfLUT.sampler();
        ibl::BRDFLUT tempLUT(device());
        tempLUT.createFallback();
        VkImage     adoptedImage     = tempLUT.image();
        VkImageView adoptedImageView = tempLUT.imageView();
        VkSampler   adoptedSampler   = tempLUT.sampler();
        tempLUT.deferDestroyImageResources();
    }
    TEST_F(BRDFLUTTest, Destructor_CleansUpResources) {
        {
            ibl::BRDFLUT brdfLUT(device());
            brdfLUT.createFallback();
        }
        SUCCEED();
    }
    TEST_F(BRDFLUTTest, Destructor_WithUninitialized_DoesNotCrash) {
        {
            ibl::BRDFLUT brdfLUT(device());
        }
        SUCCEED();
    }
    TEST_F(BRDFLUTTest, Destructor_AfterReset_DoesNotCrash) {
        {
            ibl::BRDFLUT brdfLUT(device());
            brdfLUT.createFallback();
            brdfLUT.resetToUninitialized();
        }
        SUCCEED();
    }
}  // namespace engine::test
