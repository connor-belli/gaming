// gaming.cpp : Defines the entry point for the application.
//
#define SDL_MAIN_HANDLED
#include "gaming.h"
#include "vkbuffer.h"
#include "vkctx.h"
#include "windowswapchain.h"
#include "defaultshader.h"


#include "SDL2/SDL.h"

int main()
{
	VkCtx ctx;
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("Gaming", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 600, 800, SDL_WINDOW_VULKAN);
	ctx.initVulkan(window);
	WindowSwapchain swap;
	swap.initSwapchain(window, ctx);
	ShaderModule vert(ctx, "shaders/default.vert.spv");
	ShaderModule frag(ctx, "shaders/default.frag.spv");
	DefaultLayout layout(ctx);
	DefaultShader shader(ctx, layout, vert, frag, swap.renderPass());

	// set up resources
	bool running = true;
	// event loop
	int index = 0;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type)
			{
			case SDL_QUIT:
				running = false;
				break;
			default:
				break;
			}
		}
		// Poll events, event hooks
		// Prestep physics hooks
		// step physics, contact hooks, friction hooks
		// Post step hooks 
		// Pre render hooks / tasks
		// Render
		// Post render hooks 

		VkSemaphore imageAcquired = swap.imageAcquiredSemaphore(index);
		VkSemaphore imageRendered = swap.imageRenderedSemaphore(index);


		uint32_t fi = 0;
		VkResult err = vkAcquireNextImageKHR(ctx.device(), swap.swapchain(), UINT64_MAX, imageAcquired, VK_NULL_HANDLE, &fi);
		if (err != VK_SUCCESS) {
			throw std::runtime_error("Bad swapchain");
		}
		VkCommandBuffer buf = swap.commandBuffer(fi);
		VkFence fence = swap.fence(fi);

		CHK_ERR(vkWaitForFences(ctx.device(), 1, &fence, true, UINT64_MAX));
		CHK_ERR(vkResetFences(ctx.device(), 1, &fence));
		CHK_ERR(vkResetCommandPool(ctx.device(), swap.commandPool(fi), 0));
		VkCommandBufferBeginInfo commandBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		};
		CHK_ERR(vkBeginCommandBuffer(buf, &commandBeginInfo));
		VkClearValue clearValues[] = { {1.0f, 0.5f, 1.0f, 1.0f}, {1.0f, 0} };

		VkRenderPassBeginInfo renderBeginInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = swap.renderPass(),
			.framebuffer = swap.framebuffer(fi),
			.renderArea = {
				.extent = { 600, 800 },
				},
			.clearValueCount = 2,
			.pClearValues = clearValues,
		};

		vkCmdBeginRenderPass(buf, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = {
			.width = 600,
			.height = 800,
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		VkRect2D scissor = {
			.extent = { 600, 800 },
		};
		vkCmdSetViewport(buf, 0, 1, &viewport);
		vkCmdSetScissor(buf, 0, 1, &scissor);
		vkCmdEndRenderPass(buf);
		{
			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkSubmitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &imageAcquired;
			info.pWaitDstStageMask = &wait_stage;
			info.commandBufferCount = 1;
			info.pCommandBuffers = &buf;
			info.signalSemaphoreCount = 1;
			info.pSignalSemaphores = &imageRendered;
			vkEndCommandBuffer(buf);
			vkQueueSubmit(ctx.graphicsQueue(), 1, &info, fence);
		}
		{
			VkSwapchainKHR swapchain = swap.swapchain();
			VkPresentInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			info.waitSemaphoreCount = 1;
			info.pWaitSemaphores = &imageRendered;
			info.swapchainCount = 1;
			info.pSwapchains = &swapchain;
			info.pImageIndices = &fi;
			VkResult err = vkQueuePresentKHR(ctx.graphicsQueue(), &info);
			if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
			{
				return 0;
			}
		}

		index = (index + 1) % swap.minImages();
		SDL_Delay(1);
	}
	vkDeviceWaitIdle(ctx.device());

	shader.destroy(ctx);
	layout.destroy(ctx);
	vert.destroy(ctx);
	frag.destroy(ctx);
	swap.destroy(ctx);
	ctx.destroy();
	return 0;
}
