#include "ktxImage.h"

#include <stdio.h>

#include "DefaultAllocationCallback.h"
#include "DefaultConsoleLogCallback.h"
#include "DefaultFileIOCallback.h"

#include <ktx.h>
#include <vk_funcs.h>
#include <ktxvulkan.h>
#include <vulkan/vulkan.h>

using namespace IBLLib;

KtxImage::KtxImage()
{
}

KtxImage::KtxImage(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
		// fill the create info for ktx2 (we don't support ktx 1)
	ktxTextureCreateInfo createInfo;
	createInfo.vkFormat = VK_FORMAT_R8G8B8_UNORM;
	createInfo.baseWidth = _width;
	createInfo.baseHeight = _height;
	createInfo.baseDepth = 0u;
	createInfo.numDimensions = 4u;
	createInfo.numLevels = _levels;
	createInfo.numLayers = 0u;
	createInfo.numFaces = _isCubeMap ? 6u : 1u;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	KTX_error_code result;
	result = ktxTexture2_Create(&createInfo,
															KTX_TEXTURE_CREATE_ALLOC_STORAGE,
															&m_ktxTexture);
	if(result != KTX_SUCCESS)
	{
		printf("Could not create ktx texture");
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
//	if (m_slimKTX2.setImage(_inData.data(), _inData.size(), _level, _side, 0u) != ux3d::slimktx2::Result::Success)
//	{
//		return KtxError;
//	}

	return Success;
}

Result KtxImage::save(const char* _pathOut)
{	
//	FILE* pFile = fopen(_pathOut, "wb");
//
//	if (pFile == NULL)
//	{
//		return Result::FileNotFound;
//	}
//
//	if (ux3d::slimktx2::Result::Success != m_slimKTX2.serialize(pFile))
//	{
//		fclose(pFile);
//		return Result::KtxError;
//	}
//
//	printf("Ktx file successfully written to: %s\n", _pathOut);
//
//	fclose(pFile);
	return Success;
}

