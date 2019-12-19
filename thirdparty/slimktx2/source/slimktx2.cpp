// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "slimktx2.h"
#include <stdarg.h>
#include <string.h>

using namespace ux3d::slimktx2;

SlimKTX2::SlimKTX2(const Callbacks& _callbacks) : m_callbacks(_callbacks)
{
}

SlimKTX2::~SlimKTX2()
{
	clear();
}

void SlimKTX2::clear()
{
	if (m_pLevels != nullptr)
	{
		free(m_pLevels);
		m_pLevels = nullptr;
	}
}

uint32_t SlimKTX2::getTypeSize(Format _vkFormat)
{
	switch (_vkFormat)
	{
	case Format::UNDEFINED:
		return 1u;
	case Format::R16G16B16A16_SFLOAT:
		return 2u;
	default:
		return 0u; // invalid
	}
}

uint32_t SlimKTX2::getPixelSize(Format _vkFormat)
{
	switch (_vkFormat)
	{
	case Format::R16G16B16A16_SFLOAT:
		return 8u;
	default:
		return 0u; // invalid
	}
}

uint64_t ux3d::slimktx2::SlimKTX2::getContainerSize() const
{
	uint64_t totalSize = 0u;
	const uint32_t pixelSize = getPixelSize(m_header.vkFormat);

	for (uint32_t level = 0u; level < m_header.levelCount; ++level)
	{
		uint64_t levelSize = getPixelCount(level) * pixelSize * m_header.layerCount * m_header.faceCount;
		uint32_t padding = (8u - (levelSize % 8u)) % 8u;
		totalSize += levelSize + padding;
	}

	return totalSize;
}

uint32_t SlimKTX2::getPixelCount(uint32_t _level) const
{
	uint32_t maxLevel = m_header.levelCount - 1;
	_level = maxLevel - _level;

	uint32_t result = m_header.pixelWidth >> _level;

	if (m_header.pixelHeight != 0u)
	{
		result *= m_header.pixelHeight >> _level;
	}
	if (m_header.pixelDepth != 0u)
	{
		result *= m_header.pixelDepth >> _level;
	}

	return result;
}

Result SlimKTX2::parse(IOHandle _file)
{
	clear();

	if (sizeof(Header) != read(_file, &m_header, sizeof(Header)))
	{
		return Result::IOReadFail;
	}

	if (memcmp(m_header.identifier, Header::Magic, sizeof(m_header.identifier)) != 0)
	{
		return Result::InvalidIdentifier;
	}

	if (sizeof(SectionIndex) != read(_file, &m_sections, sizeof(SectionIndex)))
	{
		return Result::IOReadFail;
	}

	const uint32_t levelCount = getLevelCount();
	const size_t levelIndexSize = sizeof(LevelIndex) * levelCount;

	m_pLevels = static_cast<LevelIndex*>(allocate(levelIndexSize));

	if (levelIndexSize != read(_file, m_pLevels, levelIndexSize))
	{
		return Result::IOReadFail;
	}

	return Result::Success;
}

Result SlimKTX2::serialize(IOHandle _file)
{
	return Result::NotImplemented;
}

uint32_t SlimKTX2::getLevelCount() const
{
	return m_header.levelCount != 0u ? m_header.levelCount : 1u;
}

uint32_t SlimKTX2::getLayerCount() const
{
	return m_header.layerCount != 0u ? m_header.layerCount : 1u;
}

const Header& SlimKTX2::getHeader() const
{
	return m_header;
}

Result SlimKTX2::specifyFormat(Format _vkFormat, uint32_t _width, uint32_t _height, uint32_t _levelCount, uint32_t _faceCount, uint32_t _depth, uint32_t _layerCount)
{
	memcpy(m_header.identifier, Header::Magic, sizeof(m_header.identifier));
	m_header.vkFormat = _vkFormat;
	m_header.typeSize = getTypeSize(_vkFormat);
	m_header.pixelWidth = _width;
	m_header.pixelHeight = _height;
	m_header.pixelDepth = _depth;
	m_header.layerCount = _layerCount;
	m_header.faceCount = _faceCount;
	m_header.levelCount = _levelCount;
	m_header.supercompressionScheme = 0u;

	// compression and sections index are not used

	const uint32_t levelCount = getLevelCount();
	const size_t levelIndexSize = sizeof(LevelIndex) * levelCount;

	m_pLevels = static_cast<LevelIndex*>(allocate(levelIndexSize));
	const uint32_t pixelSize = getPixelSize(m_header.vkFormat);

	uint32_t offset = sizeof(Header) + sizeof(SectionIndex) + levelIndexSize;

	for (uint32_t level = 0u; level < levelCount; ++level)
	{
		offset = (8u - (offset % 8u)) % 8u;

		const uint32_t pixelCount = getPixelCount(level);
		const uint32_t levelSize = pixelCount * pixelSize * m_header.faceCount;

		m_pLevels[level].byteOffset = offset;
		m_pLevels[level].byteLength = levelSize;
		m_pLevels[level].uncompressedByteLength = levelSize;

		offset += levelSize;
	}

	return Result::Success;
}

Result SlimKTX2::allocateContainer()
{
	if (m_pContainer != nullptr)
	{
		free(m_pContainer);
	}

	const uint64_t size = getContainerSize();

	m_pContainer = static_cast<uint8_t*>(allocate(size));
	
	return Result::Success;
}

Result SlimKTX2::setImage(void* _pData, size_t _byteSize, uint32_t _level, uint32_t _face, uint32_t _layer)
{
	if (_level >= m_header.levelCount)
	{
		return Result::InvalidLevelIndex;
	}
	if (_face >= m_header.faceCount)
	{
		return Result::InvalidFaceIndex;
	}

	if (_byteSize != m_pLevels[_level].byteLength)
	{
		return Result::InvalidImageSize;
	}

	uint8_t* pDestination = m_pContainer + m_pLevels[_level].byteOffset;

	memcpy(pDestination, _pData, _byteSize);

	return Result::Success;
}

void* SlimKTX2::allocate(size_t _size)
{
	return m_callbacks.allocate(m_callbacks.userData, _size);
}

void SlimKTX2::free(void* _pData)
{
	m_callbacks.free(m_callbacks.userData, _pData);
}

size_t SlimKTX2::read(IOHandle _file, void* _pData, size_t _size)
{
	return m_callbacks.read(m_callbacks.userData, _file, _pData, _size);
}

void SlimKTX2::write(IOHandle _file, const void* _pData, size_t _size)
{
	m_callbacks.write(m_callbacks.userData, _file, _pData, _size);
}

size_t SlimKTX2::tell(const IOHandle _file)
{
	return m_callbacks.tell(m_callbacks.userData, _file);
}

bool SlimKTX2::seek(IOHandle _file, size_t _offset)
{
	return m_callbacks.seek(m_callbacks.userData, _file, _offset);
}

void SlimKTX2::log(const char* _pFormat, ...)
{
	if (m_callbacks.log != nullptr)
	{
		va_list args;
		va_start(args, _pFormat);
		m_callbacks.log(m_callbacks.userData, _pFormat, args);
		va_end(args);
	}
}
