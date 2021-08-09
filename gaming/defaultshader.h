#pragma once

#include "vkctx.h"
#include "shadermodule.h"
#include "defaultlayout.h"

class DefaultShader {
private:
	VkPipeline _pipeline;
public:
	DefaultShader(const VkCtx& ctx, const DefaultLayout& layout, const ShaderModule& vertexShader, const ShaderModule& fragmentShader, VkRenderPass renderPass);
	void destroy(const VkCtx& ctx);

	VkPipeline pipeline() const { return _pipeline; }
};