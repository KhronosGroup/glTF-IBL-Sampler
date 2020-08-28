#include "ktxImage.h"

#include <stdio.h>

#include "DefaultAllocationCallback.h"
#include "DefaultConsoleLogCallback.h"
#include "DefaultFileIOCallback.h"

IBLLib::KtxImage::KtxImage()
{
	ux3d::slimktx2::Callbacks callbacks = 
		ux3d::slimktx2::DefaultAllocationCallback() |
		ux3d::slimktx2::DefaultConsoleLogCallback() |
		ux3d::slimktx2::DefaultFileIOCallback();

	m_slimKTX2.setCallbacks(callbacks);
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
	ux3d::slimktx2::Callbacks callbacks =
		ux3d::slimktx2::DefaultAllocationCallback() |
		ux3d::slimktx2::DefaultConsoleLogCallback() |
		ux3d::slimktx2::DefaultFileIOCallback();

	m_slimKTX2.setCallbacks(callbacks);
	m_slimKTX2.specifyFormat(static_cast<ux3d::slimktx2::Format>(_vkFormat), _width, _height, _levels, _isCubeMap ? 6u : 1u, 0u, 0u);
	m_slimKTX2.allocateMipLevelArray();
	m_slimKTX2.addDFDBlock(ux3d::slimktx2::DataFormatDesc::BlockHeader()); // add default dfd
}

IBLLib::Result IBLLib::KtxImage::writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level)
{
	if (m_slimKTX2.setImage(_inData.data(), _inData.size(), _level, _side, 0u) != ux3d::slimktx2::Result::Success)
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

IBLLib::KtxImage::~KtxImage()
{
}
