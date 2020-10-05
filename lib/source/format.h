#pragma once

#include <cstdint>

#include <ktx.h>
#include <vk_funcs.h>
#include <ktxvulkan.h>
#include <vulkan/vulkan.h>

namespace IBLLib
{
enum Channel : uint32_t
{
	Channel_Red = 0,
	Channel_Green,
	Channel_Blue,
	Channel_Alpha
};

// as defined by KTX (channel size)
uint32_t getTypeSize(VkFormat _vkFormat);

// as defined by vulkan (element size, block or texel)
uint32_t getFormatSize(VkFormat _vkFormat);

// get number of pixels (widht and hight) per block, returns if format is not BLOCK
bool getBlockSize(VkFormat _vkFormat, uint32_t& _outWdith, uint32_t& _outHeight);

uint32_t getChannelCount(VkFormat _vkFormat);

uint32_t getChannelSize(VkFormat _vkFormat, uint32_t _channelIndex);

int32_t getChannelIndex(VkFormat _vkFormat, Channel _channel);

bool isFloat(VkFormat _vkFormat);

bool isSigned(VkFormat _vkFormat);

bool isNormalized(VkFormat _vkFormat);

bool isSrgb(VkFormat _vkFormat);

bool isPacked(VkFormat _vkFormat);

bool isCompressed(VkFormat _vkFormat);

uint64_t getFaceSize(VkFormat _vkFormat, uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth = 0u);

// computes the pixel count (resolution) of an image of the given level
uint32_t getPixelCount(uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth);

uint32_t getPadding(uint64_t _value, uint32_t _alginment);

// greedy least common multiple
uint32_t getLCM(uint32_t _x, uint32_t _y);

// _value / offset to apply lcm4 padding to (if applicable)
uint32_t getMipPadding(uint64_t _value, VkFormat _vkFormat, bool _superCompression);

VkFormat transcodeToVkFormat(ktx_transcode_fmt_e _format, bool _sRGB);
}// IBLLib
