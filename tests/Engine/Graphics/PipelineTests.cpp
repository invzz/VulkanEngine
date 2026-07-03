#include <filesystem>
#include <gtest/gtest.h>

#include "Engine/Core/Exceptions.hpp"
#include "Engine/Graphics/Pipeline.hpp"

#include "../../fixtures/DeviceFixture.hpp"

using namespace engine;

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenViewportAndScissorConfigured) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.viewportInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    EXPECT_EQ(config.viewportInfo.viewportCount, 1);
    EXPECT_EQ(config.viewportInfo.scissorCount, 1);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenInputAssemblyUsesTriangleList) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.inputAssemblyInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    EXPECT_EQ(config.inputAssemblyInfo.topology, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    EXPECT_EQ(config.inputAssemblyInfo.primitiveRestartEnable, VK_FALSE);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenRasterizationCorrectlyConfigured) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.rasterizationInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    EXPECT_EQ(config.rasterizationInfo.depthClampEnable, VK_FALSE);
    EXPECT_EQ(config.rasterizationInfo.rasterizerDiscardEnable, VK_FALSE);
    EXPECT_EQ(config.rasterizationInfo.polygonMode, VK_POLYGON_MODE_FILL);
    EXPECT_EQ(config.rasterizationInfo.cullMode, VK_CULL_MODE_NONE);
    EXPECT_EQ(config.rasterizationInfo.frontFace, VK_FRONT_FACE_CLOCKWISE);
    EXPECT_FLOAT_EQ(config.rasterizationInfo.lineWidth, 1.0f);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenMultisampleDisabled) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.multisampleInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    EXPECT_EQ(config.multisampleInfo.rasterizationSamples, VK_SAMPLE_COUNT_1_BIT);
    EXPECT_EQ(config.multisampleInfo.sampleShadingEnable, VK_FALSE);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenColorBlendingDisabled) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.colorBlendAttachment.blendEnable, VK_FALSE);
    EXPECT_EQ(config.colorBlendAttachment.colorWriteMask, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenDepthTestEnabled) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.depthStencilInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    EXPECT_EQ(config.depthStencilInfo.depthTestEnable, VK_TRUE);
    EXPECT_EQ(config.depthStencilInfo.depthWriteEnable, VK_TRUE);
    EXPECT_EQ(config.depthStencilInfo.depthCompareOp, VK_COMPARE_OP_LESS);
}

TEST(PipelineConfigInfo, GivenDefaultConfig_WhenInspected_ThenDynamicStatesConfigured) {
    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    EXPECT_EQ(config.dynamicStateInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    EXPECT_GT(config.dynamicStateEnables.size(), 0);

    bool hasViewport = std::find(config.dynamicStateEnables.begin(), config.dynamicStateEnables.end(), VK_DYNAMIC_STATE_VIEWPORT) != config.dynamicStateEnables.end();
    bool hasScissor  = std::find(config.dynamicStateEnables.begin(), config.dynamicStateEnables.end(), VK_DYNAMIC_STATE_SCISSOR) != config.dynamicStateEnables.end();
    EXPECT_TRUE(hasViewport);
    EXPECT_TRUE(hasScissor);
}

TEST(PipelineConfigInfo, GivenMeshPipelineConfig_WhenInspected_ThenCorrectlyConfigured) {
    PipelineConfigInfo config;
    Pipeline::defaultMeshPipelineConfigInfo(config);

    EXPECT_EQ(config.viewportInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    EXPECT_EQ(config.viewportInfo.viewportCount, 1);

    bool hasViewport = std::find(config.dynamicStateEnables.begin(), config.dynamicStateEnables.end(), VK_DYNAMIC_STATE_VIEWPORT) != config.dynamicStateEnables.end();
    bool hasScissor  = std::find(config.dynamicStateEnables.begin(), config.dynamicStateEnables.end(), VK_DYNAMIC_STATE_SCISSOR) != config.dynamicStateEnables.end();
    EXPECT_TRUE(hasViewport);
    EXPECT_TRUE(hasScissor);
}

TEST(Pipeline, GivenValidShaderFile_WhenReadFile_ThenReturnsNonEmptyBuffer) {
    std::string shaderPath = "assets/shaders/compiled/post_process_vert.spv";

    if (!std::filesystem::exists(shaderPath)) {
        GTEST_SKIP() << "Shader file not found: " << shaderPath;
    }

    std::vector<char> buffer = Pipeline::readFile(shaderPath);
    EXPECT_GT(buffer.size(), 0);

    if (buffer.size() >= 4) {
        uint32_t magic = *reinterpret_cast<uint32_t*>(buffer.data());
        EXPECT_EQ(magic, 0x07230203);
    }
}

TEST(Pipeline, GivenNonexistentFile_WhenReadFile_ThenThrowsReadFileException) {
    EXPECT_THROW(Pipeline::readFile("nonexistent_shader.spv"), ReadFileException);
}

class PipelineTest : public engine::test::DeviceFixture {};

TEST_F(PipelineTest, GivenValidShadersAndConfig_WhenPipelineCreated_ThenNoThrow) {
    std::string vertPath = "assets/shaders/compiled/post_process_vert.spv";
    std::string fragPath = "assets/shaders/compiled/post_process_frag.spv";

    if (!std::filesystem::exists(vertPath) || !std::filesystem::exists(fragPath)) {
        GTEST_SKIP() << "Required shader files not found";
    }

    PipelineConfigInfo config;
    Pipeline::defaultPipelineConfigInfo(config);

    VkAttachmentDescription colorAttachment{
        .format         = VK_FORMAT_B8G8R8A8_UNORM,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachmentRef,
    };

    VkRenderPassCreateInfo renderPassInfo{
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
    };

    VkRenderPass renderPass;
    ASSERT_EQ(vkCreateRenderPass(device().device(), &renderPassInfo, nullptr, &renderPass), VK_SUCCESS);

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
    };

    VkPipelineLayout layout;
    ASSERT_EQ(vkCreatePipelineLayout(device().device(), &layoutInfo, nullptr, &layout), VK_SUCCESS);

    config.renderPass     = renderPass;
    config.pipelineLayout = layout;

    EXPECT_NO_THROW({ Pipeline pipeline(device(), vertPath, fragPath, config); });

    vkDestroyPipelineLayout(device().device(), layout, nullptr);
    vkDestroyRenderPass(device().device(), renderPass, nullptr);
}

TEST_F(PipelineTest, GivenInvalidShaderPath_WhenReadFile_ThenThrows) {
    EXPECT_THROW(Pipeline::readFile("invalid_vertex.spv"), ReadFileException);
    EXPECT_THROW(Pipeline::readFile("invalid_fragment.spv"), ReadFileException);
}
