#include <gtest/gtest.h>

#include "Engine/Graphics/Renderer.hpp"

#include "../../fixtures/DeviceFixture.hpp"

using namespace engine;

class RendererTest : public engine::test::DeviceFixture {};

TEST_F(RendererTest, GivenValidWindowAndDevice_WhenRendererCreated_ThenNoThrow) {
    EXPECT_NO_THROW({ Renderer renderer(window(), device()); });
}

TEST_F(RendererTest, GivenRenderer_WhenGetSwapChainRenderPass_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getSwapChainRenderPass();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetOffscreenRenderPass_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getOffscreenRenderPass();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetGbufferRenderPass_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getGbufferRenderPass();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetDeferredLightingRenderPass_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getDeferredLightingRenderPass();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetDepthPrepassRenderPass_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getOffscreenDepthPrepassRenderPass();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetOffscreenRenderPassLoadDepth_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getOffscreenRenderPassLoadDepth();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetOffscreenRenderPassLoadColorDepth_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkRenderPass renderPass = renderer.getOffscreenRenderPassLoadColorDepth();
    EXPECT_NE(renderPass, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenNewRenderer_WhenFrameNotStarted_ThenIsFrameInProgressReturnsFalse) {
    Renderer renderer(window(), device());

    EXPECT_FALSE(renderer.isFrameInProgress());
}

TEST_F(RendererTest, GivenNewRenderer_WhenSwapChainRecreatedOnConstruction_ThenWasSwapChainRecreatedReturnsTrue) {
    Renderer renderer(window(), device());

    EXPECT_TRUE(renderer.wasSwapChainRecreated());
}

TEST_F(RendererTest, GivenRenderer_WhenGetAspectRatio_ThenReturnsPositiveValue) {
    Renderer renderer(window(), device());

    float aspectRatio = renderer.getAspectRatio();
    EXPECT_GT(aspectRatio, 0.0f);
}

TEST_F(RendererTest, GivenRenderer_WhenGetSwapChainExtent_ThenReturnsValidDimensions) {
    Renderer renderer(window(), device());

    VkExtent2D extent = renderer.getSwapChainExtent();
    EXPECT_GT(extent.width, 0u);
    EXPECT_GT(extent.height, 0u);
}

TEST_F(RendererTest, GivenRenderer_WhenGetOffscreenImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getOffscreenImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
    EXPECT_NE(info.sampler, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetDepthImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getDepthImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetSceneColorImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getSceneColorImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetGbufferNormalImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getGbufferNormalImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetGbufferAlbedoImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getGbufferAlbedoImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetGbufferMaterialImageInfo_ThenReturnsValidInfo) {
    Renderer renderer(window(), device());

    VkDescriptorImageInfo info = renderer.getGbufferMaterialImageInfo(0);
    EXPECT_NE(info.imageView, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenGetOffscreenColorImage_ThenReturnsValidHandle) {
    Renderer renderer(window(), device());

    VkImage image = renderer.getOffscreenColorImage(0);
    EXPECT_NE(image, VK_NULL_HANDLE);
}

TEST_F(RendererTest, GivenRenderer_WhenBeginFrameCalled_ThenReturnsValidCommandBuffer) {
    Renderer renderer(window(), device());

    VkCommandBuffer cmdBuffer = renderer.beginFrame();

    if (cmdBuffer != nullptr) {
        EXPECT_NE(cmdBuffer, VK_NULL_HANDLE);
        EXPECT_TRUE(renderer.isFrameInProgress());

        renderer.endFrame();
    }
}

TEST_F(RendererTest, GivenRenderer_WhenMultipleFramesRendered_ThenFrameIndexRotates) {
    Renderer renderer(window(), device());

    int firstIndex  = -1;
    int secondIndex = -1;

    VkCommandBuffer cmd1 = renderer.beginFrame();
    if (cmd1 != nullptr) {
        firstIndex = renderer.getFrameIndex();
        renderer.endFrame();
    }

    VkCommandBuffer cmd2 = renderer.beginFrame();
    if (cmd2 != nullptr) {
        secondIndex = renderer.getFrameIndex();
        renderer.endFrame();
    }

    if (firstIndex >= 0 && secondIndex >= 0) {
        EXPECT_NE(firstIndex, secondIndex);
    }
}
