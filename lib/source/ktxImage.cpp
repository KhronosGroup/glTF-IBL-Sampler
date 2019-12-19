#include "ktxImage.h"

IBLLib::KtxImage::KtxImage() : m_slimKTX2(m_callbacks)
{
}

uint8_t* IBLLib::KtxImage::getData()
{
	uint8_t* data = nullptr;
	data = m_slimKTX2.getContainerPointer();
	return static_cast<uint8_t*>(data);
}

IBLLib::Result IBLLib::KtxImage::loadKtx2(const char* _pFilePath)
{
	FILE* pFile = fopen(_pFilePath, "r");

	if (pFile == NULL)
	{
		return Result::FileNotFound;
	}

	if (m_slimKTX2.parse(pFile) != ux3d::slimktx2::Result::Success)
	{
		return Result::KtxError;
	}

	return Result::Success;
}

IBLLib::KtxImage::KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap) :
	m_slimKTX2(m_callbacks),
	m_width(_width),
	m_height(_height),
	m_vkFormat(_vkFormat),
	m_levels(_levels),
	m_isCubeMap(_isCubeMap)
{
	m_slimKTX2.specifyFormat(static_cast<ux3d::slimktx2::Format>(m_vkFormat), m_width, m_height, m_levels, m_isCubeMap ? 6u : 1u);
	m_slimKTX2.allocateContainer();
}

IBLLib::Result IBLLib::KtxImage::writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level)
{
	// ToDo: check data size with createInfo
	//(m_createInfo.baseHeight * m_createInfo.baseWidth)>> _level

	if (m_slimKTX2.setImage(static_cast<const void*>(_inData.data()), _inData.size(), _level, _side, 0u) != ux3d::slimktx2::Result::Success)
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
		Result::FileNotFound;
	}

	if (ux3d::slimktx2::Result::Success != m_slimKTX2.serialize(pFile))
	{
		Result::KtxError;
	}

	printf("Ktx file successfully written to: %s\n", _pathOut);
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

void* IBLLib::KtxImage::allocate(size_t _size)
{
	return malloc(_size);
}

void IBLLib::KtxImage::deallocate(void* _pData)
{
	free(_pData);
}
