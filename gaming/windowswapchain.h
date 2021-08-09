#pragma once

#include <vector>

#include "pch.h"
#include "SDL2/SDL.h"
class VkCtx;

// The window swapchain class contains all the necessary framebuffers and semaphores for rendering to the window
// The window framebuffers are rendered to if antialiasing is disabled, otherwise, only rendered to as a compositing target
class WindowSwapchain {
private:
	VkSurfaceKHR _surface;
	VkSwapchainKHR _swapchain;
	VkDeviceMemory _depthAlloc;
	VkImage _depthImage;
	VkImageView _depthView;
	std::vector<VkImage> _images;
	std::vector<VkImageView> _imageViews;
	std::vector<VkFramebuffer> _frameBuffers;
	std::vector<VkCommandPool> _commandPools;
	std::vector<VkCommandBuffer> _commandBuffers;
	std::vector<VkSemaphore> _imageAcquiredSemaphores;
	std::vector<VkSemaphore> _imageRenderedSemaphores;
	std::vector<VkFence> _fences;
	VkRenderPass _renderPass;
	VkPresentModeKHR _presentMode;
	VkSurfaceFormatKHR _surfaceFormat;
	VkExtent2D _extent;
	uint32_t _minImageCount;
public:
	WindowSwapchain();
	void destroy(const VkCtx& vkctx);
	void initSwapchain(SDL_Window* window, const VkCtx& vkctx);
	VkSwapchainKHR swapchain() const { return _swapchain; }
	VkRenderPass renderPass() const { return _renderPass; }
	VkCommandBuffer commandBuffer(size_t i) const { return _commandBuffers[i]; }
	VkCommandPool commandPool(size_t i) const { return _commandPools[i]; }
	VkFramebuffer framebuffer(size_t i) const { return _frameBuffers[i]; }
	VkSemaphore imageAcquiredSemaphore(size_t i) const { return _imageAcquiredSemaphores[i]; }
	VkSemaphore imageRenderedSemaphore(size_t i) const { return _imageRenderedSemaphores[i]; }
	VkFence fence(size_t i) const { return _fences[i]; }
	size_t minImages() const { return _minImageCount; }
};