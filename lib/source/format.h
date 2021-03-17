#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace IBLLib
{
// as defined by vulkan (element size, block or texel)
uint32_t getFormatSize(VkFormat _vkFormat);

uint32_t getChannelCount(VkFormat _vkFormat);
}// IBLLib
