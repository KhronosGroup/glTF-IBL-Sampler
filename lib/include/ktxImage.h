#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "slimktx2.h"
#include "ResultType.h"

namespace IBLLib
{
	class KtxImage
	{
	public:

		KtxImage();			
		uint8_t* getData();
		IBLLib::Result loadKtx2(const char* _pFilePath);
		ux3d::slimktx2::SlimKTX2* getTextureInfo() { return &m_slimKTX2; }

		KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat,uint32_t _levels, bool _isCubeMap);
		Result writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level);
		Result save(const char* _pathOut);

		~KtxImage();
	private:
		ux3d::slimktx2::SlimKTX2 m_slimKTX2;

		uint32_t m_width = 0u;
		uint32_t m_height = 0u;
		VkFormat m_vkFormat = VkFormat::VK_FORMAT_UNDEFINED;
		uint32_t m_levels = 0u;
		bool m_isCubeMap = false;

		ux3d::slimktx2::Callbacks m_callbacks =
		{
			nullptr,
			nullptr,
			nullptr,
			nullptr,
			&writeToFile
		};

		static void writeToFile(void* _pUserData, void* _file, const void* _pData, size_t _size);
		static void* allocate(size_t _size);
		static void deallocate(void* _pData);
	};

} // !IBLLIb