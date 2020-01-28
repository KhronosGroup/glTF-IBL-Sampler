#pragma once

#include <stddef.h>
#include <vector>
#include "ResultType.h"


namespace IBLLib
{
	class Image
	{
		friend class STBImage;

	public:
		Image(const uint32_t _width, const uint32_t _height, const uint32_t _channels, const uint32_t _channelBytes, std::vector<uint8_t>& _imageData) :
			m_width(_width),
			m_height(_height),
			m_channels(_channels),
			m_channelBytes(_channelBytes),
			m_imageData(_imageData) {};

			
		IBLLib::Result applyChannelMask(const bool _channelMask[3], std::vector<uint8_t>& _data);

	private:
		const uint32_t m_width = 0u;
		const uint32_t m_height = 0u;
		const uint32_t m_channels = 0u;
		const uint32_t m_channelBytes = 0u;

		std::vector<uint8_t>& m_imageData;
	
	};


} // !IBLLib
