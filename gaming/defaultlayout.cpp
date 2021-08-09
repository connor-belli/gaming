#include "defaultlayout.h"

#include "defaultvertex.h"

DefaultLayout::DefaultLayout(const VkCtx& ctx) 
	: _layout(VK_NULL_HANDLE)
{
	VkDescriptorSetLayoutBinding uniformBinding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &uniformBinding,
	};
	CHK_ERR(vkCreateDescriptorSetLayout(ctx.device(), &descriptorLayoutInfo, nullptr, &_descriptorLayout));

	VkPipelineLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &_descriptorLayout,
	};
	CHK_ERR(vkCreatePipelineLayout(ctx.device(), &info, nullptr, &_layout));
}

void DefaultLayout::destroy(const VkCtx& ctx)
{
	vkDestroyPipelineLayout(ctx.device(), _layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.device(), _descriptorLayout, nullptr);
}
