#include <gtest/gtest.h>

#include "vulkan/graphics_commands.h"

TEST(GraphicsCommands, ImageUsesKeepSpecializedLayouts) {
    using namespace nk::vk;
    EXPECT_EQ(image_access(ImageUse::discard).layout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(image_access(ImageUse::sampled).layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    EXPECT_EQ(image_access(ImageUse::color_attachment).layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(image_access(ImageUse::depth_attachment).layout, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(image_access(ImageUse::present).layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

TEST(GraphicsCommands, UploadScopesDescribeTransferAndShaderAccess) {
    using namespace nk::vk;
    const auto write = image_access(ImageUse::transfer_destination);
    const auto read = image_access(ImageUse::transfer_source);
    const auto sample = image_access(ImageUse::sampled);
    EXPECT_EQ(write.scope.stages, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(write.scope.access, VK_ACCESS_TRANSFER_WRITE_BIT);
    EXPECT_EQ(read.scope.stages, VK_PIPELINE_STAGE_TRANSFER_BIT);
    EXPECT_EQ(read.scope.access, VK_ACCESS_TRANSFER_READ_BIT);
    EXPECT_NE(sample.scope.stages & VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    EXPECT_NE(sample.scope.stages & VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0);
    EXPECT_EQ(sample.scope.access, VK_ACCESS_SHADER_READ_BIT);
}

TEST(GraphicsCommands, AttachmentScopesIncludeReadsAndWrites) {
    using namespace nk::vk;
    const auto color = image_access(ImageUse::color_attachment).scope;
    const auto depth = image_access(ImageUse::depth_attachment).scope;
    EXPECT_EQ(color.stages, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    EXPECT_EQ(color.access, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    EXPECT_EQ(depth.stages, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    EXPECT_EQ(depth.access, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
}
