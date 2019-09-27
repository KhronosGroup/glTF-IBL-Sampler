#include "STBImage.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

IBLLib::STBImage::~STBImage()
{
	if (m_byteData != nullptr)
	{
		stbi_image_free(m_byteData);
		m_byteData = nullptr;
	}
}

IBLLib::Result IBLLib::STBImage::saveHdr(const char* _path, int _width, int _height, int _channels, const void* data)
{
	return stbi_write_hdr(_path, _width, _height, _channels, (const float* )data) == 0 ? StbError : Success;
}

IBLLib::Result IBLLib::STBImage::savePng(const char* _path, int _width, int _height, int _channels, const void* data)
{	
	return stbi_write_png(_path, _width, _height, _channels, data, _width * _channels) == 0 ? StbError : Success;
}

IBLLib::Result IBLLib::STBImage::loadHdr(const char* _path)
{
	if (m_hdrData != nullptr)
	{
		return InvalidArgument;
	}

	// stbi_loadf
	m_hdrData = stbi_loadf(_path, &m_width, &m_height, &m_channels, STBI_rgb_alpha);

	if (m_hdrData == nullptr)
	{
		printf("Failed to load image %s\n", _path);
		printf("STB: %s\n", stbi_failure_reason());
		return StbError;
	}
	else
	{
		printf("Successfully loaded %s ", _path);
		printf("%d x %d x %d \n", m_width, m_height, m_channels);
		m_isHdr = true;
		return Success;
	}
}

size_t IBLLib::STBImage::getByteSize() const
{
	if(m_isHdr)
		return (size_t)m_width * (size_t)m_height * STBI_rgb_alpha * sizeof(float);
	else
		return (size_t)m_width * (size_t)m_height * STBI_rgb;
}		

IBLLib::Result IBLLib::STBImage::loadPng(const char* _path)
{ 
	if (m_byteData != nullptr)
	{
		return InvalidArgument;
	}

	m_byteData = stbi_load(_path, &m_width, &m_height, &m_channels, STBI_rgb);

	if (m_byteData == nullptr)
	{
		printf("Failed to load image %s\n", _path);
		printf("STB: %s\n", stbi_failure_reason());
		return StbError;
	}
	else
	{
		printf("Successfully loaded %s ", _path);
		printf("%d x %d x %d \n", m_width, m_height, m_channels);
		m_isHdr = false;
		return Success;
	}
}