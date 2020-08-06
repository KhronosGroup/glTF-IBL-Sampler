#pragma once
#include "ResultType.h"
#include <stddef.h>

namespace IBLLib
{
	class STBImage
	{
	public:
		STBImage() = default;
		~STBImage();

		Result saveHdr(const char* _path, int _width, int _height, int _channels, const void* data);
		Result savePng(const char* _path, int _width, int _height, int _channels, const void* data);

		// loads .hdr images
		Result loadHdr(const char* _path);
		Result loadPng(const char* _path);
		size_t getByteSize() const;

		const float* getHdrData() const { return m_hdrData; }
		const unsigned char* getByteData() const { return m_byteData; }

		const int getWidth() const { return m_width; }
		const int getHeight() const { return m_height; }
		const int getChannels() const { return m_channels; }

	private:
		int m_width = 0u;
		int m_height = 0u;
		int m_channels = 0u;

		union 
		{
			float* m_hdrData = nullptr;
			unsigned char* m_byteData;
		};

		bool m_isHdr = false;
	};
} // !IBLLib