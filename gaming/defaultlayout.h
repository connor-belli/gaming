#pragma once

#include "vkctx.h"

class DefaultLayout {
private:
	VkDescriptorSetLayout _descriptorLayout;
	VkPipelineLayout _layout;
public:
	DefaultLayout(const VkCtx& ctx);
	void destroy(const VkCtx& ctx);

	VkPipelineLayout layout() const { return _layout; }
};