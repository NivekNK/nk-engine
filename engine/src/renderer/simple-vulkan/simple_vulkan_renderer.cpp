#include "nkpch.h"

#include "simple_vulkan_renderer.h"

namespace nk {
    void SimpleVulkanRenderer::on_resized(u32 width, u32 height) {
        m_window_resized = true;
    }

    void SimpleVulkanRenderer::init() {
        m_device.init(m_platform);
        recreate_swap_chain();
        create_command_buffers();

        m_global_pool = lve::LveDescriptorPool::Builder(m_device)
                            .setMaxSets(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT)
                            .build();

        load_game_objects();

        m_ubo_buffers.resize(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < m_ubo_buffers.size(); i++) {
            m_ubo_buffers[i] = std::make_unique<lve::LveBuffer>(
                m_device,
                sizeof(lve::GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            m_ubo_buffers[i]->map();
        }

        m_global_set_layout = lve::LveDescriptorSetLayout::Builder(m_device)
                                  .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                                  .build();

        m_global_descriptor_sets.resize(lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < m_global_descriptor_sets.size(); i++) {
            auto bufferInfo = m_ubo_buffers[i]->descriptorInfo();
            lve::LveDescriptorWriter(*m_global_set_layout, *m_global_pool)
                .writeBuffer(0, &bufferInfo)
                .build(m_global_descriptor_sets[i]);
        }

        m_simple_render_system = std::make_unique<lve::SimpleRenderSystem>(m_device, m_swap_chain->getRenderPass(), m_global_set_layout->getDescriptorSetLayout());
        m_point_light_system = std::make_unique<lve::PointLightSystem>(m_device, m_swap_chain->getRenderPass(), m_global_set_layout->getDescriptorSetLayout());

        m_viewer_object = lve::LveGameObject::createGameObject();
        m_viewer_object.transform.translation.z = -2.5f;

        // lve::KeyboardMovementController cameraController{};

        m_current_time = std::chrono::high_resolution_clock::now();
    }

    void SimpleVulkanRenderer::shutdown() {
        vkDeviceWaitIdle(m_device.device());
        free_command_buffers();
    }

    bool SimpleVulkanRenderer::draw_frame(const RenderPacket& packet) {
        auto newTime = std::chrono::high_resolution_clock::now();
        float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - m_current_time).count();
        m_current_time = newTime;

        // cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
        m_camera.setViewYXZ(m_viewer_object.transform.translation, m_viewer_object.transform.rotation);

        float aspect = m_swap_chain->extentAspectRatio();
        m_camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

        if (begin_frame(0)) {
            int frameIndex = m_current_frame_index;
            lve::FrameInfo frameInfo{
                frameIndex,
                frameTime,
                m_current_command_buffer,
                m_camera,
                m_global_descriptor_sets[frameIndex],
                m_game_objects};

            // update
            lve::GlobalUbo ubo{};
            ubo.projection = m_camera.getProjection();
            ubo.view = m_camera.getView();
            ubo.inverseView = m_camera.getInverseView();
            m_point_light_system->update(frameInfo, ubo);
            m_ubo_buffers[frameIndex]->writeToBuffer(&ubo);
            m_ubo_buffers[frameIndex]->flush();

            // render
            begin_swap_chain_render_pass(m_current_command_buffer);

            // order here matters
            m_simple_render_system->renderGameObjects(frameInfo);
            m_point_light_system->render(frameInfo);

            end_swap_chain_render_pass(m_current_command_buffer);
            end_frame(0);
        }

        return true;
    }

    bool SimpleVulkanRenderer::begin_frame(f64 delta_time) {
        assert(!m_is_frame_started && "Can't call beginFrame while already in progress");

        auto result = m_swap_chain->acquireNextImage(&m_current_image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swap_chain();
            return false;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            Assert(false, "failed to acquire swap chain image!");
        }

        m_is_frame_started = true;

        auto command_buffer = m_command_buffers[m_current_image_index];
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(command_buffer, &beginInfo) != VK_SUCCESS) {
            Assert(false, "failed to begin recording command buffer!");
        }

        m_current_command_buffer = command_buffer;

        return true;
    }

    bool SimpleVulkanRenderer::end_frame(f64 delta_time) {
        assert(m_is_frame_started && "Can't call endFrame while frame is not in progress");
        auto command_buffer = m_command_buffers[m_current_image_index];
        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            Assert(false, "failed to record command buffer!");
        }

        auto result = m_swap_chain->submitCommandBuffers(&command_buffer, &m_current_image_index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window_resized) {
            m_window_resized = false;
            recreate_swap_chain();
        } else if (result != VK_SUCCESS) {
            Assert(false, "failed to present swap chain image!");
        }

        m_is_frame_started = false;
        m_current_frame_index = (m_current_frame_index + 1) % lve::LveSwapChain::MAX_FRAMES_IN_FLIGHT;

        return true;
    }

    void SimpleVulkanRenderer::begin_swap_chain_render_pass(VkCommandBuffer command_buffer) {
        assert(m_is_frame_started && "Can't call beginSwapChainRenderPass if frame is not in progress");
        assert(command_buffer == m_current_command_buffer &&
               "Can't begin render pass on command buffer from a different frame");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_swap_chain->getRenderPass();
        renderPassInfo.framebuffer = m_swap_chain->getFrameBuffer(m_current_image_index);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_swap_chain->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swap_chain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(m_swap_chain->getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, m_swap_chain->getSwapChainExtent()};
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }

    void SimpleVulkanRenderer::end_swap_chain_render_pass(VkCommandBuffer command_buffer) {
        assert(m_is_frame_started && "Can't call endSwapChainRenderPass if frame is not in progress");
        assert(command_buffer == m_current_command_buffer &&
               "Can't end render pass on command buffer from a different frame");
        vkCmdEndRenderPass(command_buffer);
    }

    void SimpleVulkanRenderer::create_command_buffers() {
        // Allocate one command buffer per swap chain image
        m_command_buffers.resize(m_swap_chain->imageCount());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_device.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_command_buffers.size());

        if (vkAllocateCommandBuffers(m_device.device(), &allocInfo, m_command_buffers.data()) != VK_SUCCESS) {
            Assert(false, "failed to allocate command buffers!");
        }
    }

    void SimpleVulkanRenderer::free_command_buffers() {
        if (m_command_buffers.empty()) {
            return;
        }
        vkFreeCommandBuffers(
            m_device.device(),
            m_device.getCommandPool(),
            static_cast<uint32_t>(m_command_buffers.size()),
            m_command_buffers.data());
        m_command_buffers.clear();
    }

    void SimpleVulkanRenderer::recreate_swap_chain() {
        vkDeviceWaitIdle(m_device.device());

        // Free existing command buffers before recreating swap chain
        if (!m_command_buffers.empty()) {
            free_command_buffers();
        }

        if (m_swap_chain == nullptr) {
            m_swap_chain = std::make_unique<lve::LveSwapChain>(m_device, VkExtent2D{m_platform->width(), m_platform->height()});
        } else {
            std::shared_ptr<lve::LveSwapChain> old_swap_chain = std::move(m_swap_chain);
            m_swap_chain = std::make_unique<lve::LveSwapChain>(m_device, VkExtent2D{m_platform->width(), m_platform->height()}, old_swap_chain);
            if (!old_swap_chain->compareSwapFormats(*m_swap_chain.get())) {
                Assert(false, "Swap chain image(or depth) format has changed!");
            }
        }

        // Recreate command buffers with the new swap chain image count
        create_command_buffers();
    }

    void SimpleVulkanRenderer::load_game_objects() {
        std::shared_ptr<lve::LveModel> lveModel = lve::LveModel::createModelFromFile(m_device, "assets/models/little_dragon.obj");
        auto flatVase = lve::LveGameObject::createGameObject();
        flatVase.model = lveModel;
        flatVase.transform.translation = {-.5f, 0.25f, 0.f};
        flatVase.transform.scale = {0.4f, 0.4f, 0.4f};
        flatVase.transform.rotation = {glm::pi<float>(), glm::radians(15.f), 0.f};
        m_game_objects.emplace(flatVase.getId(), std::move(flatVase));

        lveModel = lve::LveModel::createModelFromFile(m_device, "assets/models/little_dragon.obj");
        auto smoothVase = lve::LveGameObject::createGameObject();
        smoothVase.model = lveModel;
        smoothVase.transform.translation = {.5f, 0.25f, 0.f};
        smoothVase.transform.scale = {0.3f, 0.3f, 0.3f};
        smoothVase.transform.rotation = {glm::pi<float>(), glm::radians(-15.f), 0.f};
        m_game_objects.emplace(smoothVase.getId(), std::move(smoothVase));

        lveModel = lve::LveModel::createModelFromFile(m_device, "assets/models/quad.obj");
        auto floor = lve::LveGameObject::createGameObject();
        floor.model = lveModel;
        floor.transform.translation = {0.f, .5f, 0.f};
        floor.transform.scale = {3.f, 1.f, 3.f};
        m_game_objects.emplace(floor.getId(), std::move(floor));

        std::vector<glm::vec3> lightColors{
            {1.f, .1f, .1f},
            {.1f, .1f, 1.f},
            {.1f, 1.f, .1f},
            {1.f, 1.f, .1f},
            {.1f, 1.f, 1.f},
            {1.f, 1.f, 1.f} //
        };

        for (int i = 0; i < lightColors.size(); i++) {
            auto pointLight = lve::LveGameObject::makePointLight(0.2f);
            pointLight.color = lightColors[i];
            auto rotateLight = glm::rotate(
                glm::mat4(1.f),
                (i * glm::two_pi<float>()) / lightColors.size(),
                {0.f, -1.f, 0.f});
            pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            m_game_objects.emplace(pointLight.getId(), std::move(pointLight));
        }
    }
}
