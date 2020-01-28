#include "Image.h"


IBLLib::Result IBLLib::Image::applyChannelMask(const bool _channelMask[3], std::vector<uint8_t>& _data)
{
	if (m_channels != 3)
	{
		printf("Error: invalid channel count\n");
		return Result::InvalidArgument;
	}

	_data.clear();

	for (uint32_t x = 0; x < m_width; x++)
	{
		for (uint32_t y = 0; y < m_height; y++)
		{
			for (uint32_t c = 0; c < m_channels; c++)
			{
				
				if(_channelMask[c])
				{ 
					uint32_t imageIdx = m_channels * (x * m_width + y) + c;
					_data.push_back(m_imageData.at(imageIdx));
				}
				else
				{
					_data.push_back(0);
				}
			}
		}
	}

	return Result::Success;
}
