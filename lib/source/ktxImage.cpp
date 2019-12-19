#include "ktxImage.h"

IBLLib::KtxImage::KtxImage() : m_slimKTX2(ux3d::slimktx2::Callbacks())
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
	return Result::KtxError;
}

IBLLib::KtxImage::KtxImage(Version _version, uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap) :
	m_version(_version),
	m_slimKTX2(ux3d::slimktx2::Callbacks())
{

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
	
	printf("Ktx file successfully written to: %s\n", _pathOut);

	return IBLLib::KtxError;
}

IBLLib::KtxImage::~KtxImage()
{
}
