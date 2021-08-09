#include "windowswapchain.h"

#include <algorithm>
#include <string>
#include <iostream>
#include <stdexcept>

#include "vkctx.h"
#include "SDL2/SDL_vulkan.h"

static bool isVsync = true;
constexpr bool isAA = false;

VkExtent2D chooseSwapExtent(SDL_Window* window, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VkPresentModeKHR getPresentMode() {
	return isVsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

WindowSwapchain::WindowSwapchain() : _surface(VK_NULL_HANDLE),
 _swapchain(VK_NULL_HANDLE),
    _depthAlloc(VK_NULL_HANDLE),
    _depthImage(VK_NULL_HANDLE),
   _depthView(VK_NULL_HANDLE),
    _renderPass(VK_NULL_HANDLE),
    _presentMode(VK_PRESENT_MODE_MAILBOX_KHR),
    _surfaceFormat({}),
    _extent({0, 0}),
    _minImageCount(2)
{

}

void WindowSwapchain::destroy(const VkCtx& vkctx)
{
    for (int i = 0; i < _commandBuffers.size(); i++) {
        vkFreeCommandBuffers(vkctx.device(), _commandPools[i], 1, &_commandBuffers[i]);
        vkDestroyCommandPool(vkctx.device(), _commandPools[i], nullptr);
        vkDestroySemaphore(vkctx.device(), _imageAcquiredSemaphores[i], nullptr);
        vkDestroySemaphore(vkctx.device(), _imageRenderedSemaphores[i], nullptr);
        vkDestroyFence(vkctx.device(), _fences[i], nullptr);
    }

    for (VkFramebuffer buffer : _frameBuffers) {
        vkDestroyFramebuffer(vkctx.device(), buffer, nullptr);
    }
    vkDestroyImageView(vkctx.device(), _depthView, nullptr);
    for (VkImageView view : _imageViews) {
        vkDestroyImageView(vkctx.device(), view, nullptr);
    }
    vkDestroyImage(vkctx.device(), _depthImage, nullptr);
    vkFreeMemory(vkctx.device(), _depthAlloc, nullptr);
    vkDestroyRenderPass(vkctx.device(), _renderPass, nullptr);
    vkDestroySwapchainKHR(vkctx.device(), _swapchain, nullptr);
	vkDestroySurfaceKHR(vkctx.instance(), _surface, nullptr);
}


void WindowSwapchain::initSwapchain(SDL_Window* window, const VkCtx& vkctx)
{
	if (!SDL_Vulkan_CreateSurface(window, vkctx.instance(), &_surface)) {
        std::cout << SDL_GetError() << std::endl;
		throw std::runtime_error(SDL_GetError());
	}
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkctx.physicalDevice(), _surface, &surfaceCaps);
    _presentMode = getPresentMode();
    uint32_t size = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(vkctx.physicalDevice(), _surface, &size, nullptr);
    std::vector<VkSurfaceFormatKHR> formats (size);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkctx.physicalDevice(), _surface, &size, formats.data());
    _surfaceFormat = chooseSwapSurfaceFormat(formats);
    _extent = chooseSwapExtent(window, surfaceCaps);
    VkBool32 supported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(vkctx.physicalDevice(), vkctx.graphicsQueueIndex(), _surface, &supported);
    if (!supported) {
        throw std::runtime_error("Unsupported surface");
    }
    if (surfaceCaps.minImageCount < 2) {
        surfaceCaps.minImageCount = 2;
    }

    uint32_t queueIndex = vkctx.graphicsQueueIndex();
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = _surface,
        .minImageCount = surfaceCaps.minImageCount,
        .imageFormat = _surfaceFormat.format,
        .imageColorSpace = _surfaceFormat.colorSpace,
        .imageExtent = _extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &queueIndex,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = _presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = nullptr,
    };

    CHK_ERR(vkCreateSwapchainKHR(vkctx.device(), &createInfo, nullptr, &_swapchain));

    vkGetSwapchainImagesKHR(vkctx.device(), _swapchain, &size, nullptr);
    _images.resize(size);
    vkGetSwapchainImagesKHR(vkctx.device(), _swapchain, &size, _images.data());

    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    if (!isAA) {
        // Init depth format
        VkImageCreateInfo depthImageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthFormat,
            .extent = {.width = _extent.width, .height = _extent.height, .depth = 1,},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        CHK_ERR(vkCreateImage(vkctx.device(), &depthImageInfo, nullptr, &_depthImage));

        VkMemoryRequirements depthReqs{};
        vkGetImageMemoryRequirements(vkctx.device(), _depthImage, &depthReqs);
        VkMemoryAllocateInfo depthMemInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = depthReqs.size,
            .memoryTypeIndex = findMemoryType(vkctx, depthReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        CHK_ERR(vkAllocateMemory(vkctx.device(), &depthMemInfo, nullptr, &_depthAlloc));
        vkBindImageMemory(vkctx.device(), _depthImage, _depthAlloc, 0);

        VkImageViewCreateInfo depthViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = _depthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depthFormat,
            .components = { }, // VK_COMPONENT_SWIZZLE_IDENTITY == 0 therefore zero struct
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
        CHK_ERR(vkCreateImageView(vkctx.device(), &depthViewInfo, nullptr, &_depthView));
    }

    VkAttachmentDescription colorDesc = {
        .format = _surfaceFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT, // Cannot multisample in window framebuffer, composited to single sample
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorRef = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentDescription depthDesc = {
        .format = depthFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthRef = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDependency subpassDep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = isAA?VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT: VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };

    VkSubpassDescription subpassDesc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef,
    };

    if (!isAA) {
        subpassDesc.pDepthStencilAttachment = &depthRef;
    }

    VkAttachmentDescription descs[] = {
        colorDesc,
        depthDesc,
    };

    VkRenderPassCreateInfo renderpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = isAA?1:2,
        .pAttachments = descs,
        .subpassCount = 1,
        .pSubpasses = &subpassDesc,
        .dependencyCount = 1,
        .pDependencies = &subpassDep,
    };

    CHK_ERR(vkCreateRenderPass(vkctx.device(), &renderpassInfo, nullptr, &_renderPass));
    _imageViews.resize(size);
    _frameBuffers.resize(size);
    _commandPools.resize(size);
    _commandBuffers.resize(size);
    _imageAcquiredSemaphores.resize(size);
    _imageRenderedSemaphores.resize(size);
    _fences.resize(size);
    for (int i = 0; i < size; i++) {
        VkImageViewCreateInfo imageViewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = _images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = _surfaceFormat.format,
            .components = { }, // VK_COMPONENT_SWIZZLE_IDENTITY == 0 therefore zero struct
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }
        };
        CHK_ERR(vkCreateImageView(vkctx.device(), &imageViewInfo, nullptr, &_imageViews[i]));
        VkImageView views[] = { _imageViews[i], _depthView };
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = _renderPass,
            .attachmentCount = isAA?1:2,
            .pAttachments = views,
            .width = _extent.width,
            .height = _extent.height,
            .layers = 1,
        };
        CHK_ERR(vkCreateFramebuffer(vkctx.device(), &framebufferInfo, nullptr, &_frameBuffers[i]));

        {
            VkCommandPoolCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = vkctx.graphicsQueueIndex(),
            };
            CHK_ERR(vkCreateCommandPool(vkctx.device(), &info, nullptr, &_commandPools[i]));
        }
        {
            VkCommandBufferAllocateInfo info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = _commandPools[i],
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            CHK_ERR(vkAllocateCommandBuffers(vkctx.device(), &info, &_commandBuffers[i]));
        }
        {
            VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            CHK_ERR(vkCreateSemaphore(vkctx.device(), &info, nullptr, &_imageAcquiredSemaphores[i]));
            CHK_ERR(vkCreateSemaphore(vkctx.device(), &info, nullptr, &_imageRenderedSemaphores[i]));
        }
        {
            VkFenceCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT,
            };
            vkCreateFence(vkctx.device(), &info, nullptr, &_fences[i]);
        }
    }
}
