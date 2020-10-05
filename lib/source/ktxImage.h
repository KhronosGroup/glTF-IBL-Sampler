#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "ResultType.h"

struct ktxTexture2;

namespace IBLLib
{
	class KtxImage
	{
	public:
		// use this constructor if you want to load a ktx file
		KtxImage();
		// use this constructor if you want to create a ktx file
		KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap);
		~KtxImage();

		Result loadKtx2(const char* _pFilePath);

		Result writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level);
		Result save(const char* _pathOut);

		uint32_t getWidth() const;
		uint32_t getHeight() const;
		uint32_t getLevels() const;
		bool isCubeMap() const;
		VkFormat getFormat() const;

	private:
		ktxTexture2* m_ktxTexture = nullptr;
	};

} // !IBLLIb
