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
		enum class Version
		{
			KTX1 = 0,
			KTX2 = 1,
		};

		KtxImage();			
		uint8_t* getData();
		IBLLib::Result loadKtx2(const char* _pFilePath);
		ux3d::slimktx2::SlimKTX2* getTextureInfo() { return &m_slimKTX2; }

		KtxImage(Version _version, uint32_t _width, uint32_t _height, VkFormat _vkFormat,uint32_t _levels, bool _isCubeMap);
		Result writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level);
		Result save(const char* _pathOut);

		~KtxImage();
	private:
		Version m_version = Version::KTX2;
		ux3d::slimktx2::SlimKTX2 m_slimKTX2;

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