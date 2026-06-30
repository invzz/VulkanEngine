#include <gtest/gtest.h>

#include "Engine/Systems/LightingRenderBindings.hpp"

#include "../../fixtures/DeviceFixture.hpp"

using namespace engine;

// =============================================================================
// LightingRenderBindings Construction Tests
// =============================================================================

class LightingRenderBindingsTest : public engine::test::DeviceFixture {};

TEST_F(LightingRenderBindingsTest, GivenValidDevice_WhenConstructed_ThenNoThrow) {
    EXPECT_NO_THROW({ LightingRenderBindings lrb(device()); });
}

TEST_F(LightingRenderBindingsTest, GivenNewBindings_WhenResourcesNotCreated_ThenLayoutsAreNull) {
    LightingRenderBindings lrb(device());

    EXPECT_EQ(lrb.getShadowDescriptorSetLayout(), VK_NULL_HANDLE);
    EXPECT_EQ(lrb.getIBLDescriptorSetLayout(), VK_NULL_HANDLE);
}

TEST_F(LightingRenderBindingsTest, GivenBindings_WhenCreateResourcesCalled_ThenShadowLayoutIsValid) {
    LightingRenderBindings lrb(device());
    lrb.createResources();

    EXPECT_NE(lrb.getShadowDescriptorSetLayout(), VK_NULL_HANDLE);
}

TEST_F(LightingRenderBindingsTest, GivenBindings_WhenCreateResourcesCalled_ThenIBLLayoutIsValid) {
    LightingRenderBindings lrb(device());
    lrb.createResources();

    EXPECT_NE(lrb.getIBLDescriptorSetLayout(), VK_NULL_HANDLE);
}

TEST_F(LightingRenderBindingsTest, GivenBindings_WhenSetShadowSystemCalledWithNull_ThenNoThrow) {
    LightingRenderBindings lrb(device());
    lrb.createResources();

    EXPECT_NO_THROW(lrb.setShadowSystem(nullptr));
}

TEST_F(LightingRenderBindingsTest, GivenBindings_WhenSetIBLSystemCalledWithNull_ThenNoThrow) {
    LightingRenderBindings lrb(device());
    lrb.createResources();

    EXPECT_NO_THROW(lrb.setIBLSystem(nullptr));
}
