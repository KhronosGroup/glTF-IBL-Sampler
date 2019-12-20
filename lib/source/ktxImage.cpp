#include "ktxImage.h"
#include <stdio.h>
#include <stdlib.h>

IBLLib::KtxImage::KtxImage()
{
	ux3d::slimktx2::Callbacks callbacks{};

	callbacks.allocate = allocate;
	callbacks.free = deallocate;
	callbacks.write = writeToFile;

	m_slimKTX2.setCallbacks(callbacks);
}

uint8_t* IBLLib::KtxImage::getData()
{
	return m_slimKTX2.getLevelContainerPointer();
}

IBLLib::Result IBLLib::KtxImage::loadKtx2(const char* _pFilePath)
{
	FILE* pFile = fopen(_pFilePath, "r");

	if (pFile == NULL)
	{
		printf("Could not open file '%s'\n", _pFilePath);
		return Result::FileNotFound;
	}

	if (m_slimKTX2.parse(pFile) != ux3d::slimktx2::Result::Success)
	{
		return Result::KtxError;
	}

	return Result::Success;
}

IBLLib::KtxImage::KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
	ux3d::slimktx2::Callbacks callbacks{};

	callbacks.allocate = allocate;
	callbacks.free = deallocate;
	callbacks.write = writeToFile;

	m_slimKTX2.setCallbacks(callbacks);
	m_slimKTX2.specifyFormat(static_cast<ux3d::slimktx2::Format>(_vkFormat), _width, _height, _levels, _isCubeMap ? 6u : 1u);
	m_slimKTX2.allocateLevelContainer();
}

IBLLib::Result IBLLib::KtxImage::writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level)
{
	// ToDo: check data size with createInfo
	//(m_createInfo.baseHeight * m_createInfo.baseWidth)>> _level

	if (m_slimKTX2.setImage(_inData.data(), _inData.size(), _level, _side, 0u) != ux3d::slimktx2::Result::Success)
	{
		return KtxError;
	}

	return Success;
}


IBLLib::Result IBLLib::KtxImage::save(const char* _pathOut)
{	
	FILE* pFile = fopen(_pathOut, "w");

	if (pFile == NULL)
	{
		return Result::FileNotFound;
	}

	if (ux3d::slimktx2::Result::Success != m_slimKTX2.serialize(pFile))
	{
		fclose(pFile);
		return Result::KtxError;
	}

	printf("Ktx file successfully written to: %s\n", _pathOut);

	fclose(pFile);
	return Success;
}

void IBLLib::KtxImage::writeToFile(void* _pUserData, void* _file, const void* _pData, size_t _size)
{
	FILE* pFile = static_cast<FILE*>(_file);
	fwrite(_pData, sizeof(uint8_t), _size, pFile);
}

IBLLib::KtxImage::~KtxImage()
{
}

void* IBLLib::KtxImage::allocate(void* _pUserData, size_t _size)
{
	return malloc(_size);
}

void IBLLib::KtxImage::deallocate(void* _pUserData, void* _pData)
{
	free(_pData);
}
