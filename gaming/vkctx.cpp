#include "vkctx.h"

#include <algorithm>
#include <bitset>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

uint32_t findMemoryType(const VkCtx& ctx, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

#ifndef NDEBUG
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}
#endif

void printDeviceProps(const VkPhysicalDeviceProperties& props, VkPhysicalDevice dev) {
	std::cout << "Found device " << props.deviceName << std::endl;
	VkPhysicalDeviceMemoryProperties mem;
	vkGetPhysicalDeviceMemoryProperties(dev, &mem);
	std::cout << "Memory Types:" << std::endl;
	for (int i = 0; i < mem.memoryTypeCount; i++) {
		const VkMemoryType& type = mem.memoryTypes[i];
		const VkMemoryHeap &heap = mem.memoryHeaps[type.heapIndex];
		std::cout << "Heap size: " << heap.size / (1024*1024) << std::endl;
		std::cout << "Memory Flags Bits: " << std::bitset<16>(type.propertyFlags) << std::endl;
		std::cout << std::endl;
	}
}

void CHK_ERR(VkResult result) {
	if (result < VK_SUCCESS) {
		throw std::runtime_error(std::to_string(result));
	}
}

VkCtx::VkCtx()
	: _instance(VK_NULL_HANDLE),
	_physicalDevice(VK_NULL_HANDLE),
	_device(VK_NULL_HANDLE),
	_graphicsQueue(VK_NULL_HANDLE),
	_graphicsQueueIndex(0),
	_allocator(VMA_NULL)
{
}

void VkCtx::initVulkan(SDL_Window* window)
{
	VkApplicationInfo appInfo = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Gaming",
		.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
		.pEngineName = "Gaming Engine",
		.engineVersion = VK_MAKE_VERSION(0, 1, 0),
		.apiVersion = VK_API_VERSION_1_2,
	};

	std::vector<const char*> extensions;
	uint32_t extensionCount = 0;
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
	extensions.resize(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

#ifdef NDEBUG
	std::vector<const char*> layers;
#else
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
#endif

	VkInstanceCreateInfo instanceInfo = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledLayerCount = (uint32_t)layers.size(),
		.ppEnabledLayerNames = layers.data(),
		.enabledExtensionCount = (uint32_t)extensions.size(),
		.ppEnabledExtensionNames = extensions.data(),
	};

	CHK_ERR(vkCreateInstance(&instanceInfo, nullptr, &_instance));

#ifndef NDEBUG
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr; // Optional

	if (CreateDebugUtilsMessengerEXT(_instance, &createInfo, nullptr, &_debugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
#endif

	uint32_t nDevs = 0;
	vkEnumeratePhysicalDevices(_instance, &nDevs, nullptr);
	std::vector<VkPhysicalDevice> physicalDevs(nDevs);
	vkEnumeratePhysicalDevices(_instance, &nDevs, physicalDevs.data());

	if (physicalDevs.size() == 0) {
		throw std::runtime_error("No Vulkan capable devices found");
	}

	int currentPriority = -1;
	VkPhysicalDevice currentDev = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties currentProps;
	for (VkPhysicalDevice dev : physicalDevs) {
		VkPhysicalDeviceProperties props;
		int priority = 0;
		vkGetPhysicalDeviceProperties(dev, &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			priority++;
		}
		if (priority > currentPriority) {
			currentPriority = priority;
			currentDev = dev;
			currentProps = props;
		}
	}
	if (currentDev == VK_NULL_HANDLE) {
		throw std::runtime_error("No suitable vulkan device found");
	}
	_physicalDevice = currentDev;

	printDeviceProps(currentProps, _physicalDevice);

	uint32_t nQueues = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(currentDev, &nQueues, nullptr);
	std::vector<VkQueueFamilyProperties> queueProps(nQueues);
	vkGetPhysicalDeviceQueueFamilyProperties(currentDev, &nQueues, queueProps.data());
	if (nQueues == 0) {
		throw std::runtime_error("No queue families availible");
	}

	bool suitableQueueFound = false;
	uint32_t queueIndex = 0;
	for (int i = 0; i < nQueues; i++) {
		VkQueueFamilyProperties props = queueProps[i];
		if (!(~props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT)) && !suitableQueueFound) {
			suitableQueueFound = true;
			queueIndex = i;
		}
	}

	if (!suitableQueueFound) {
		throw std::runtime_error("No suitable transfer/graphics queue availible");
	}

	_graphicsQueueIndex = queueIndex;
	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = queueIndex,
		.queueCount = 1,
		.pQueuePriorities = &priority,
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkDeviceCreateInfo devInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueInfo,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = nullptr,
	};

	CHK_ERR(vkCreateDevice(_physicalDevice, &devInfo, nullptr, &_device));
	vkGetDeviceQueue(_device, queueIndex, 0, &_graphicsQueue);

	{
		VmaAllocatorCreateInfo allocatorInfo = {
			.physicalDevice = _physicalDevice,
			.device = _device,
			.instance = _instance,
			.vulkanApiVersion = VK_API_VERSION_1_0,
		};

		CHK_ERR(vmaCreateAllocator(&allocatorInfo, &_allocator));
	}
}

void VkCtx::destroy()
{
	vkDestroyDevice(_device, nullptr);
#ifndef NDEBUG
	DestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
#endif
	vkDestroyInstance(_instance, nullptr);
}
