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
		KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat,uint32_t _levels, bool _isCubeMap);
		~KtxImage();

		uint8_t* getData();
		IBLLib::Result loadKtx2(const char* _pFilePath);

		Result writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level);
		Result save(const char* _pathOut);

		uint32_t getWidth() const { return m_slimKTX2.getHeader().pixelWidth; }
		uint32_t getHeight() const { return m_slimKTX2.getHeader().pixelHeight; }
		uint32_t getLevels() const { return m_slimKTX2.getHeader().levelCount; }
		bool isCubeMap() const { return m_slimKTX2.getHeader().faceCount == 6u; }
		VkFormat getFormat() const { return static_cast<VkFormat>(m_slimKTX2.getHeader().vkFormat); }
		uint64_t getImageDataSize() const { return m_slimKTX2.getContainerSize(); }

	private:
		ux3d::slimktx2::SlimKTX2 m_slimKTX2;
			   
		static void writeToFile(void* _pUserData, void* _file, const void* _pData, size_t _size);
		static void* allocate(void* _pUserData, size_t _size);
		static void deallocate(void* _pUserData, void* _pData);
	};

} // !IBLLIb