#pragma once

#include "pch.h"

struct SDL_Window;

void CHK_ERR(VkResult result);


class VkCtx {
private:
	VkInstance _instance;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueIndex;
	VmaAllocator _allocator;
#ifndef NDEBUG
	VkDebugUtilsMessengerEXT _debugMessenger;

#endif
public:
	VkCtx();
	void initVulkan(SDL_Window* window);
	void destroy();
	VkInstance instance() const { return _instance; };
	VkDevice device() const { return _device; };
	VkPhysicalDevice physicalDevice() const { return _physicalDevice; }
	uint32_t graphicsQueueIndex() const { return _graphicsQueueIndex; };
	VkQueue graphicsQueue() const { return _graphicsQueue; };
	VmaAllocator allocator() const { return _allocator; }
};

uint32_t findMemoryType(const VkCtx& ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties);