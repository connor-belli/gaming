#pragma once

#include <string>

#include "vkctx.h"


class ShaderModule {
private:
	VkShaderModule _module;
public:
	ShaderModule(const VkCtx& ctx, std::string filename);
	void destroy(const VkCtx& ctx);
	VkShaderModule shaderModule() const { return _module; }
};