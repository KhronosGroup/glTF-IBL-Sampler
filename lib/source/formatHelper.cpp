#include "formatHelper.h"
#include <../other_include/GL/glew.h>
#include <map>


namespace IBLLib
{

	struct VULKAN_FORMAT_MAPPING {
		uint32_t glFormat;
	
	};

	const std::map<VkFormat, VULKAN_FORMAT_MAPPING> glFormatTable =
	{
	{VK_FORMAT_R8G8B8_UNORM,						 {GL_RGB8}},
	{VK_FORMAT_R16G16B16_SFLOAT,                   {GL_RGB16F}},
	{VK_FORMAT_R32G32B32_SFLOAT,                   {GL_RGB32F}},

	{VK_FORMAT_R8G8B8A8_UNORM,						  {GL_RGBA8}},
	{VK_FORMAT_R16G16B16A16_SFLOAT,                   {GL_RGBA16F}},
	{VK_FORMAT_R32G32B32A32_SFLOAT,                   {GL_RGBA32F}},
	};

}//!IBLLib



uint32_t IBLLib::vulkanToGlFormat(VkFormat vulkanFormat)
{
	auto it = glFormatTable.find(vulkanFormat);
	
	if (it == glFormatTable.end())
	{
		printf("Vulkan format not found");
		
		return 0;
	}
	
	return it->second.glFormat;
}