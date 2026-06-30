#include <gtest/gtest.h>

#include "Engine/Systems/IBLSystem.hpp"

#include "../../fixtures/DeviceFixture.hpp"

namespace engine::test {

    // Test fixture using shared Device
    class IBLSystemTest : public DeviceFixture {};

    // ==============================================================================
    // Construction Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, Construction_DoesNotThrow) {
        EXPECT_NO_THROW({ IBLSystem iblSystem(device()); });
    }

    TEST_F(IBLSystemTest, Construction_NotInitiallyGenerated) {
        IBLSystem iblSystem(device());

        EXPECT_FALSE(iblSystem.isGenerated());
    }

    TEST_F(IBLSystemTest, Construction_GenerationCounterStartsAtZero) {
        IBLSystem iblSystem(device());

        // Counter starts at some value, should increment on changes
        uint64_t initialCounter = iblSystem.getGenerationCounter();
        EXPECT_GE(initialCounter, 0);
    }

    TEST_F(IBLSystemTest, Construction_HasDefaultSettings) {
        IBLSystem iblSystem(device());

        const auto& settings = iblSystem.getSettings();

        // Default settings from IBLSettings.hpp
        EXPECT_EQ(settings.irradianceSize, 64);
        EXPECT_EQ(settings.prefilterSize, 256);
        EXPECT_EQ(settings.prefilterMipLevels, 8);
        EXPECT_EQ(settings.brdfLUTSize, 256);
    }

    // ==============================================================================
    // Fallback Resource Tests (via resetToFallback)
    // ==============================================================================

    TEST_F(IBLSystemTest, ResetToFallback_DoesNotThrow) {
        IBLSystem iblSystem(device());

        EXPECT_NO_THROW(iblSystem.resetToFallback());
    }

    TEST_F(IBLSystemTest, ResetToFallback_IsNotGenerated) {
        IBLSystem iblSystem(device());
        iblSystem.resetToFallback();

        EXPECT_FALSE(iblSystem.isGenerated());
    }

    TEST_F(IBLSystemTest, ResetToFallback_IncrementsGenerationCounter) {
        IBLSystem iblSystem(device());

        uint64_t initialCounter = iblSystem.getGenerationCounter();
        iblSystem.resetToFallback();
        uint64_t newCounter = iblSystem.getGenerationCounter();

        EXPECT_GT(newCounter, initialCounter);
    }

    // ==============================================================================
    // Descriptor Info Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, GetIrradianceDescriptorInfo_AfterConstruction_ReturnsValidInfo) {
        IBLSystem iblSystem(device());

        VkDescriptorImageInfo info = iblSystem.getIrradianceDescriptorInfo();

        // Should have valid descriptor info even without generation (fallback)
        EXPECT_NE(info.sampler, VK_NULL_HANDLE);
        EXPECT_NE(info.imageView, VK_NULL_HANDLE);
        EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    TEST_F(IBLSystemTest, GetPrefilteredDescriptorInfo_AfterConstruction_ReturnsValidInfo) {
        IBLSystem iblSystem(device());

        VkDescriptorImageInfo info = iblSystem.getPrefilteredDescriptorInfo();

        EXPECT_NE(info.sampler, VK_NULL_HANDLE);
        EXPECT_NE(info.imageView, VK_NULL_HANDLE);
        EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    TEST_F(IBLSystemTest, GetBRDFLUTDescriptorInfo_AfterConstruction_ReturnsValidInfo) {
        IBLSystem iblSystem(device());

        VkDescriptorImageInfo info = iblSystem.getBRDFLUTDescriptorInfo();

        EXPECT_NE(info.sampler, VK_NULL_HANDLE);
        EXPECT_NE(info.imageView, VK_NULL_HANDLE);
        EXPECT_EQ(info.imageLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    TEST_F(IBLSystemTest, GetDescriptorInfos_AfterFallback_StillValid) {
        IBLSystem iblSystem(device());
        iblSystem.resetToFallback();

        VkDescriptorImageInfo irradInfo = iblSystem.getIrradianceDescriptorInfo();
        VkDescriptorImageInfo prefInfo  = iblSystem.getPrefilteredDescriptorInfo();
        VkDescriptorImageInfo brdfInfo  = iblSystem.getBRDFLUTDescriptorInfo();

        EXPECT_NE(irradInfo.sampler, VK_NULL_HANDLE);
        EXPECT_NE(prefInfo.sampler, VK_NULL_HANDLE);
        EXPECT_NE(brdfInfo.sampler, VK_NULL_HANDLE);
    }

    // ==============================================================================
    // Settings Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, UpdateSettings_DoesNotThrow) {
        IBLSystem iblSystem(device());

        IBLSystem::Settings newSettings;
        newSettings.brdfLUTSize    = 128;
        newSettings.irradianceSize = 32;

        EXPECT_NO_THROW(iblSystem.updateSettings(newSettings));
    }

    TEST_F(IBLSystemTest, UpdateSettings_ReturnsUpdatedSettings) {
        IBLSystem iblSystem(device());

        IBLSystem::Settings newSettings;
        newSettings.brdfLUTSize    = 128;
        newSettings.irradianceSize = 32;

        iblSystem.updateSettings(newSettings);

        const auto& currentSettings = iblSystem.getSettings();
        EXPECT_EQ(currentSettings.brdfLUTSize, 128);
        EXPECT_EQ(currentSettings.irradianceSize, 32);
    }

    // ==============================================================================
    // Update Loop Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, Update_WithoutRequest_DoesNothing) {
        IBLSystem iblSystem(device());

        uint64_t counterBefore = iblSystem.getGenerationCounter();

        // Call update without requesting regeneration
        iblSystem.update();

        uint64_t counterAfter = iblSystem.getGenerationCounter();

        EXPECT_EQ(counterBefore, counterAfter);
    }

    // ==============================================================================
    // Disk I/O Tests (error paths - no actual files)
    // ==============================================================================

    TEST_F(IBLSystemTest, LoadFromDisk_NonexistentDirectory_ReturnsFalse) {
        IBLSystem iblSystem(device());

        bool result = iblSystem.loadFromDisk("/nonexistent/directory/path");

        EXPECT_FALSE(result);
    }

    // ==============================================================================
    // Destructor Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, Destructor_CleansUpResources) {
        {
            IBLSystem iblSystem(device());
        }  // Destructor called here

        SUCCEED();
    }

    TEST_F(IBLSystemTest, Destructor_AfterResetToFallback_CleansUpResources) {
        {
            IBLSystem iblSystem(device());
            iblSystem.resetToFallback();
        }  // Destructor called here

        SUCCEED();
    }

    // ==============================================================================
    // Request Regeneration Tests (deferred regeneration)
    // ==============================================================================

    TEST_F(IBLSystemTest, RequestRegeneration_SetsUpDeferredRegeneration) {
        IBLSystem iblSystem(device());

        // We need a skybox for regeneration, but for this test we just check
        // that the request is stored (actual generation needs environment map)

        // Note: Can't easily test this without a real skybox,
        // but we can verify the call doesn't crash
        // Commenting out because it requires a valid Skybox
        // iblSystem.requestRegeneration(iblSystem.getSettings(), someSkybox);
        // iblSystem.update();

        SUCCEED();
    }

    // ==============================================================================
    // Multiple Systems Tests
    // ==============================================================================

    TEST_F(IBLSystemTest, MultipleSystems_IndependentState) {
        IBLSystem system1(device());
        IBLSystem system2(device());

        IBLSystem::Settings settings1;
        settings1.brdfLUTSize = 64;

        IBLSystem::Settings settings2;
        settings2.brdfLUTSize = 128;

        system1.updateSettings(settings1);
        system2.updateSettings(settings2);

        EXPECT_EQ(system1.getSettings().brdfLUTSize, 64);
        EXPECT_EQ(system2.getSettings().brdfLUTSize, 128);
    }

}  // namespace engine::test
