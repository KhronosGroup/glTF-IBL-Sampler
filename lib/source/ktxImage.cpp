#include "ktxImage.h"

IBLLib::KtxImage::KtxImage() 
{
	ux3d::slimktx2::Callbacks callbacks{};

	callbacks.allocate = allocate;
	callbacks.free = deallocate;
	callbacks.write = writeToFile;
	callbacks.read = readFromFile;
	callbacks.tell = tell;

	m_slimKTX2.setCallbacks(callbacks);
}

uint8_t* IBLLib::KtxImage::getData()
{
	return m_slimKTX2.getContainerPointer();
}

IBLLib::Result IBLLib::KtxImage::loadKtx2(const char* _pFilePath)
{
	FILE* pFile = fopen(_pFilePath, "rb");

	if (pFile == NULL)
	{
		printf("Could not open file '%s'\n", _pFilePath);
		return Result::FileNotFound;
	}

	if (m_slimKTX2.parse(pFile) != ux3d::slimktx2::Result::Success)
	{
		fclose(pFile);
		return Result::KtxError;
	}

	fclose(pFile);
	return Result::Success;
}

IBLLib::KtxImage::KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
	ux3d::slimktx2::Callbacks callbacks{};

	callbacks.allocate = allocate;
	callbacks.free = deallocate;
	callbacks.write = writeToFile;
	callbacks.read = readFromFile;
	callbacks.tell = tell;
	
	m_slimKTX2.setCallbacks(callbacks);
	m_slimKTX2.specifyFormat(static_cast<ux3d::slimktx2::Format>(_vkFormat), _width, _height, _levels, _isCubeMap ? 6u : 1u);
	m_slimKTX2.allocateContainer();
}

IBLLib::Result IBLLib::KtxImage::writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level)
{
	const uint32_t ktxLevel = getLevels() - _level - 1u;
	if (m_slimKTX2.setImage(_inData.data(), _inData.size(), ktxLevel, _side, 0u) != ux3d::slimktx2::Result::Success)
	{
		return KtxError;
	}

	return Success;
}

IBLLib::Result IBLLib::KtxImage::save(const char* _pathOut)
{	
	FILE* pFile = fopen(_pathOut, "wb");

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

size_t IBLLib::KtxImage::readFromFile(void* _pUserData, void* _file, void* _pData, size_t _size)
{
	FILE* pFile = static_cast<FILE*>(_file);
	return fread(_pData, sizeof(uint8_t), _size, pFile);
}

size_t IBLLib::KtxImage::tell(void* _pUserData, void* _file)
{
	FILE* pFile = static_cast<FILE*>(_file);
	return ftell(pFile);
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
