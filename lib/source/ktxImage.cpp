#include "ktxImage.h"

#include <stdio.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <vulkan/vulkan.h>

#include <cassert>

using namespace IBLLib;

KtxImage::KtxImage()
{
}

KtxImage::KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
		// fill the create info for ktx2 (we don't support ktx 1)
	ktxTextureCreateInfo createInfo;
	createInfo.vkFormat = _vkFormat;
	createInfo.baseWidth = _width;
	createInfo.baseHeight = _height;
	createInfo.baseDepth = 1u;
	createInfo.numDimensions = 2u;
	createInfo.numLevels = _levels;
	createInfo.numLayers = 1u;
	createInfo.numFaces = _isCubeMap ? 6u : 1u;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	KTX_error_code result;
	result = ktxTexture2_Create(&createInfo,
															KTX_TEXTURE_CREATE_ALLOC_STORAGE,
															&m_ktxTexture);
	if(result != KTX_SUCCESS)
	{
		printf("Could not create ktx texture\n");
		m_ktxTexture = nullptr;
	}
}

KtxImage::~KtxImage()
{
	ktxTexture_Destroy(ktxTexture(m_ktxTexture));
}

Result KtxImage::loadKtx2(const char* _pFilePath)
{
	assert(((void)"m_ktxTexture must be uninitialized.", m_ktxTexture != nullptr));

	KTX_error_code result;
	result = ktxTexture2_CreateFromNamedFile(_pFilePath,
																					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
																					&m_ktxTexture);

	if(result != KTX_SUCCESS)
	{
		printf("Could not load ktx file at %s \n", _pFilePath);
	}

	return Result::Success;
}

Result KtxImage::writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level)
{
	KTX_error_code result = ktxTexture_SetImageFromMemory(ktxTexture(m_ktxTexture), _level, 0u, _side, _inData.data(), _inData.size());

	if(result != KTX_SUCCESS)
	{
		printf("Could not write image data to ktx texture\n");
		return Result::KtxError;
	}

	return Success;
}

Result KtxImage::save(const char* _pathOut)
{

	KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(m_ktxTexture), _pathOut);

	if(result != KTX_SUCCESS)
	{
		printf("Could not write ktx file\n");
		return Result::KtxError;
	}

	return Success;
}

uint32_t KtxImage::getWidth() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->baseWidth;
}

uint32_t KtxImage::getHeight() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->baseHeight;
}

uint32_t KtxImage::getLevels() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->numLevels;
}

bool KtxImage::isCubeMap() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->numFaces == 6u;
}

VkFormat KtxImage::getFormat() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return static_cast<VkFormat>(m_ktxTexture->vkFormat);
}
