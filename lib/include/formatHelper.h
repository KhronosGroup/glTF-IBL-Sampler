#pragma once

#include <vulkan/vulkan.h>



namespace IBLLib
{

	uint32_t vulkanToGlFormat(VkFormat vulkanFormat);
	VkFormat glToVulkanFormat(uint32_t _glFormat);


}//!IBLLib