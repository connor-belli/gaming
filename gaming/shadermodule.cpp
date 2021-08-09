#include "shadermodule.h"

#include <fstream>
#include <stdexcept>

ShaderModule::ShaderModule(const VkCtx& ctx, std::string filename) :
	_module(VK_NULL_HANDLE)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Could not file file for shader module");
	}
	size_t size = file.tellg();
	file.seekg(0, file.beg);	
	char* buff = new char[size];
	file.read(buff, size);
	file.close();

	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = reinterpret_cast<uint32_t*>(buff),
	};

	CHK_ERR(vkCreateShaderModule(ctx.device(), &info, nullptr, &_module));

	delete[] buff;
}

void ShaderModule::destroy(const VkCtx& ctx)
{
	vkDestroyShaderModule(ctx.device(), _module, nullptr);
}
