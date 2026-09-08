#include <gtest/gtest.h>

#include "renderer/render_target.h"

namespace {
    constexpr nk::RenderAttachmentConfig color_attachment{
        .role = nk::RenderAttachmentRole::color,
        .source = nk::RenderAttachmentSource::window_color,
        .format = nk::TextureFormat::bgra8_unorm,
        .load = nk::RenderLoadOperation::clear,
        .store = nk::RenderStoreOperation::store,
        .final_use = nk::RenderAttachmentUse::present,
        .resize = nk::RenderAttachmentResize::window,
    };
    constexpr nk::RenderAttachmentConfig depth_attachment{
        .role = nk::RenderAttachmentRole::depth,
        .source = nk::RenderAttachmentSource::window_depth,
        .format = nk::TextureFormat::depth32_float,
        .load = nk::RenderLoadOperation::clear,
        .store = nk::RenderStoreOperation::discard,
        .final_use = nk::RenderAttachmentUse::depth_stencil_attachment,
        .resize = nk::RenderAttachmentResize::window,
    };

    nk::RenderPassConfig pass_with(
        const nk::cl::slice<const nk::RenderAttachmentConfig> attachments) {
        return {
            .name = "world",
            .area = {0, 0, 1280, 720},
            .attachments = attachments,
        };
    }

    nk::Texture texture(
        const nk::TextureFormat format,
        const nk::TextureFlag flags) {
        return {
            .width = 1280,
            .height = 720,
            .channel_count = 4,
            .format = format,
            .flags = flags,
            .generation = 3,
            .m_internal_data = reinterpret_cast<void*>(0x1),
        };
    }
}

TEST(RenderPassConfig, RejectsMissingAndDuplicateAttachments) {
    auto empty = pass_with({});
    auto empty_result = nk::validate_render_pass_config(empty);
    ASSERT_FALSE(empty_result);
    EXPECT_EQ(
        empty_result.error(),
        nk::render_target_error::invalid_attachment_count);

    const nk::RenderAttachmentConfig duplicate[]{
        color_attachment,
        color_attachment,
    };
    auto duplicate_result = nk::validate_render_pass_config(
        pass_with({duplicate}));
    ASSERT_FALSE(duplicate_result);
    EXPECT_EQ(
        duplicate_result.error(),
        nk::render_target_error::duplicate_attachment_role);
}

TEST(RenderPassConfig, RejectsRoleFormatAndSourceMismatches) {
    nk::RenderAttachmentConfig invalid = color_attachment;
    invalid.format = nk::TextureFormat::depth32_float;
    auto format = nk::validate_render_pass_config(pass_with({&invalid, 1}));
    ASSERT_FALSE(format);
    EXPECT_EQ(format.error(), nk::render_target_error::incompatible_format);

    invalid = color_attachment;
    invalid.source = nk::RenderAttachmentSource::window_depth;
    auto source = nk::validate_render_pass_config(pass_with({&invalid, 1}));
    ASSERT_FALSE(source);
    EXPECT_EQ(source.error(), nk::render_target_error::incompatible_source);

    const nk::RenderAttachmentConfig mixed_samples[]{
        color_attachment,
        [] {
            auto attachment = depth_attachment;
            attachment.sample_count = nk::TextureSampleCount::four;
            return attachment;
        }(),
    };
    auto samples = nk::validate_render_pass_config(pass_with({mixed_samples}));
    ASSERT_FALSE(samples);
    EXPECT_EQ(
        samples.error(),
        nk::render_target_error::incompatible_sample_count);
}

TEST(RenderPassConfig, ExposesStablePipelineSignature) {
    const nk::RenderAttachmentConfig configs[]{
        color_attachment,
        depth_attachment,
    };
    auto signature = nk::render_pass_signature(pass_with({configs}));
    ASSERT_TRUE(signature);
    EXPECT_EQ(signature->color_format, nk::TextureFormat::bgra8_unorm);
    EXPECT_EQ(
        signature->depth_stencil_format,
        nk::TextureFormat::depth32_float);
    EXPECT_EQ(signature->sample_count, nk::TextureSampleCount::one);
    EXPECT_TRUE(signature->valid());
}

TEST(RenderPassConfig, AcceptsOffscreenColorAttachments) {
    nk::RenderAttachmentConfig offscreen = color_attachment;
    offscreen.source = nk::RenderAttachmentSource::texture;
    offscreen.format = nk::TextureFormat::r32_uint;
    offscreen.final_use = nk::RenderAttachmentUse::transfer_source;
    auto validated = nk::validate_render_pass_config(
        pass_with({&offscreen, 1}));
    EXPECT_TRUE(validated);
}

TEST(RenderPassConfig, OwnsBuiltinWindowPassDescriptionsOutsideBackend) {
    nk::WindowRenderPasses passes;
    ASSERT_TRUE(passes.init(
        1280,
        720,
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFormat::depth32_float));
    EXPECT_EQ(passes.world().area.width, 1280u);
    EXPECT_EQ(passes.world().attachments.length(), 2u);
    EXPECT_EQ(
        passes.world().attachments[0].final_use,
        nk::RenderAttachmentUse::color_attachment);
    EXPECT_EQ(passes.ui().attachments.length(), 1u);
    EXPECT_EQ(
        passes.ui().attachments[0].final_use,
        nk::RenderAttachmentUse::present);

    EXPECT_FALSE(passes.resize(0, 720));
    EXPECT_EQ(passes.world().area.width, 1280u);
    ASSERT_TRUE(passes.resize(800, 600));
    EXPECT_EQ(passes.world().area.width, 800u);
    EXPECT_EQ(passes.ui().area.height, 600u);
}

TEST(TextureFormat, DescribesIntegerPickingTexels) {
    EXPECT_TRUE(nk::is_color_format(nk::TextureFormat::r32_uint));
    EXPECT_FALSE(nk::is_depth_format(nk::TextureFormat::r32_uint));
    EXPECT_EQ(
        nk::texture_format_texel_size(nk::TextureFormat::r32_uint),
        4u);
    constexpr auto usage = nk::TextureUsage::color_attachment |
        nk::TextureUsage::transfer_source;
    EXPECT_TRUE(nk::has_usage(usage, nk::TextureUsage::color_attachment));
    EXPECT_TRUE(nk::has_usage(usage, nk::TextureUsage::transfer_source));
    EXPECT_FALSE(nk::has_usage(usage, nk::TextureUsage::sampled));
}

TEST(RenderPassConfig, RejectsInvalidFinalUsesAndResizePolicies) {
    nk::RenderAttachmentConfig attachment = color_attachment;
    attachment.final_use =
        nk::RenderAttachmentUse::depth_stencil_attachment;
    auto final_use = nk::validate_render_pass_config(
        pass_with({&attachment, 1}));
    ASSERT_FALSE(final_use);
    EXPECT_EQ(
        final_use.error(),
        nk::render_target_error::incompatible_final_use);

    attachment = color_attachment;
    attachment.resize = nk::RenderAttachmentResize::fixed;
    auto resize = nk::validate_render_pass_config(
        pass_with({&attachment, 1}));
    ASSERT_FALSE(resize);
    EXPECT_EQ(resize.error(), nk::render_target_error::incompatible_source);
}

TEST(RenderTarget, RequiresDeclaredTextureUsages) {
    const nk::RenderAttachmentConfig configs[]{color_attachment};
    auto pass = pass_with({configs});
    nk::Texture color = texture(
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFlag::writable | nk::TextureFlag::external);
    color.usage = nk::TextureUsage::sampled;
    nk::Texture* attachments[]{&color};

    auto initialized = nk::RenderTarget{}.init(
        pass,
        {1280, 720, {attachments}});
    ASSERT_FALSE(initialized);
    EXPECT_EQ(
        initialized.error(),
        nk::render_target_error::incompatible_usage);
}

TEST(RenderTarget, ValidatesResourcesAndDetectsStaleGenerations) {
    const nk::RenderAttachmentConfig configs[]{
        color_attachment,
        depth_attachment,
    };
    auto pass = pass_with({configs});
    nk::Texture color = texture(
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFlag::writable | nk::TextureFlag::external);
    nk::Texture depth = texture(
        nk::TextureFormat::depth32_float,
        nk::TextureFlag::writable);
    nk::Texture* attachments[]{&color, &depth};

    nk::RenderTarget target;
    ASSERT_TRUE(target.init(pass, {1280, 720, {attachments}}));
    EXPECT_TRUE(target.valid());
    EXPECT_TRUE(target.current());
    EXPECT_EQ(target.attachment(nk::RenderAttachmentRole::color), &color);
    EXPECT_EQ(target.attachment(nk::RenderAttachmentRole::depth), &depth);

    ++depth.generation;
    EXPECT_FALSE(target.current());
}

TEST(RenderTarget, PreservesPublishedTargetWhenRebuildFails) {
    const nk::RenderAttachmentConfig configs[]{color_attachment};
    auto pass = pass_with({configs});
    nk::Texture color = texture(
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFlag::writable | nk::TextureFlag::external);
    nk::Texture invalid = color;
    invalid.width = 1;
    nk::Texture* original_attachments[]{&color};
    nk::Texture* invalid_attachments[]{&invalid};
    nk::RenderTarget target;

    ASSERT_TRUE(target.init(pass, {1280, 720, {original_attachments}}));
    auto rebuild = target.init(pass, {1280, 720, {invalid_attachments}});

    ASSERT_FALSE(rebuild);
    EXPECT_EQ(
        rebuild.error(),
        nk::render_target_error::incompatible_dimensions);
    EXPECT_TRUE(target.valid());
    EXPECT_TRUE(target.current());
    EXPECT_EQ(target.attachment(nk::RenderAttachmentRole::color), &color);
}

TEST(RenderTarget, DetectsMetadataChangesWithoutGenerationChange) {
    const nk::RenderAttachmentConfig configs[]{color_attachment};
    auto pass = pass_with({configs});
    nk::Texture color = texture(
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFlag::writable | nk::TextureFlag::external);
    nk::Texture* attachments[]{&color};
    nk::RenderTarget target;

    ASSERT_TRUE(target.init(pass, {1280, 720, {attachments}}));
    color.sample_count = nk::TextureSampleCount::four;
    EXPECT_FALSE(target.current());

    color.sample_count = nk::TextureSampleCount::one;
    color.flags = nk::TextureFlag::writable;
    EXPECT_FALSE(target.current());
}

TEST(RenderTarget, RejectsDimensionsSamplesFormatsAndOwnership) {
    const nk::RenderAttachmentConfig configs[]{color_attachment};
    auto pass = pass_with({configs});
    nk::Texture color = texture(
        nk::TextureFormat::bgra8_unorm,
        nk::TextureFlag::writable | nk::TextureFlag::external);
    nk::Texture* attachments[]{&color};
    nk::RenderTarget target;

    color.width = 1;
    auto dimensions = target.init(pass, {1280, 720, {attachments}});
    ASSERT_FALSE(dimensions);
    EXPECT_EQ(
        dimensions.error(),
        nk::render_target_error::incompatible_dimensions);

    color.width = 1280;
    color.sample_count = nk::TextureSampleCount::four;
    auto samples = target.init(pass, {1280, 720, {attachments}});
    ASSERT_FALSE(samples);
    EXPECT_EQ(
        samples.error(),
        nk::render_target_error::incompatible_sample_count);

    color.sample_count = nk::TextureSampleCount::one;
    color.format = nk::TextureFormat::rgba8_unorm;
    auto format = target.init(pass, {1280, 720, {attachments}});
    ASSERT_FALSE(format);
    EXPECT_EQ(format.error(), nk::render_target_error::incompatible_format);

    color.format = nk::TextureFormat::bgra8_unorm;
    color.flags = nk::TextureFlag::writable;
    auto source = target.init(pass, {1280, 720, {attachments}});
    ASSERT_FALSE(source);
    EXPECT_EQ(source.error(), nk::render_target_error::incompatible_source);
}
