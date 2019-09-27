#include "ktxImage.h"
#include "formatHelper.h"

IBLLib::KtxImage::KtxImage(Version _version, uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap) :
	m_version(_version)
{

	switch (_version)
	{
	case Version::KTX1:
		m_createInfo.glInternalformat = IBLLib::vulkanToGlFormat(_vkFormat);
		break;
	case Version::KTX2:
		m_createInfo.vkFormat = _vkFormat;
		break;

		
	}

	m_createInfo.baseWidth = _width;
	m_createInfo.baseHeight = _height;
	m_createInfo.baseDepth = 1u;
	m_createInfo.numDimensions = 2u;
	
	m_createInfo.numLevels = _levels;
	m_createInfo.numLayers = 1u;
	m_createInfo.numFaces = _isCubeMap ? 6u : 1u;
	m_createInfo.isArray = KTX_FALSE;
	m_createInfo.generateMipmaps = KTX_FALSE;

	ktxTexture1* pTexture1 = nullptr;
	ktxTexture2* pTexture2 = nullptr;

	KTX_error_code result = KTX_SUCCESS;
	
	switch (_version)
	{
	case Version::KTX1:
		result = ktxTexture1_Create(&m_createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &pTexture1);
		m_pTexture = ktxTexture(pTexture1);
		break;
	case Version::KTX2:
		result = ktxTexture2_Create(&m_createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &pTexture2);
		if (result == KTX_SUCCESS)
		{ 
			const char name[] = "ktxfile";
			ktxHashList_AddKVPair(&pTexture2->kvDataHead, KTX_WRITER_KEY, sizeof(name), name);
			m_pTexture = ktxTexture(pTexture2);
		}
		break;
	}

	if (result != KTX_SUCCESS)
	{
		ktxTexture_Destroy(m_pTexture);
		m_pTexture = nullptr;
		printf("ktx: %s\n", ktxErrorString(result));
	}
}

IBLLib::Result IBLLib::KtxImage::writeFace(const std::vector<unsigned char>& _inData, uint32_t _side, uint32_t _level)
{
	// ToDo: check data size with createInfo
	//(m_createInfo.baseHeight * m_createInfo.baseWidth)>> _level

	if (m_pTexture == nullptr || _inData.empty())
	{
		return InvalidArgument;
	}

	const uint32_t  layer = 0u;// we have no array, just cubemap sides

	KTX_error_code result = ktxTexture_SetImageFromMemory(m_pTexture, _level, layer, _side, _inData.data(), _inData.size() );

	if (result != KTX_SUCCESS)
	{
		printf("ktx: %s\n", ktxErrorString(result));
		return KtxError;
	}

	return Success;
}

IBLLib::Result IBLLib::KtxImage::compress(ktxBasisParams _params)
{
	if (m_pTexture == nullptr)
	{
		return InvalidArgument;
	}

	if (m_version != Version::KTX2)
	{
		return KtxError;
	}

	printf("Compressing ...\n");

	KTX_error_code result = ktxTexture2_CompressBasisEx(reinterpret_cast<ktxTexture2*>(m_pTexture), &_params);

	if (result != KTX_SUCCESS)
	{
		ktxTexture_Destroy(ktxTexture(m_pTexture));
		printf("ktx: %s\n", ktxErrorString(result));
		m_pTexture = nullptr;
		return KtxError;
	}

	return Success;
}

IBLLib::Result IBLLib::KtxImage::compress(unsigned int _qualityLevel)
{
	ktxBasisParams ktxParams{};
	ktxParams.structSize = sizeof(ktxBasisParams);
	ktxParams.threadCount = 1;
	ktxParams.compressionLevel = 1;	//encoding speed vs. quality - Range is 0 - 5, default is 1. Higher values are slower, but give higher quality
	ktxParams.qualityLevel = _qualityLevel;	// Range is 1 - 255 Lower gives better	compression/lower quality/faster
	ktxParams.maxEndpoints = 0;		//set the maximum number of color endpoint clusters	from 1 - 16128. Default is 0
	ktxParams.endpointRDOThreshold = 1.25f;//Lower is higher quality but less quality per output bit(try 1.0 - 3.0)
	ktxParams.maxSelectors = 0;		//Manually set the maximum number of color selector clusters from 1 - 16128. Default is 0, unset.
	ktxParams.selectorRDOThreshold = 1.5f;//Set selector RDO quality threshold. The default is 1.5. Lower is higher quality but less quality per output bit(try 1.0 - 3.0).
	ktxParams.normalMap = false;
	ktxParams.separateRGToRGB_A = false;
	ktxParams.preSwizzle = false;
	ktxParams.noEndpointRDO = false;
	ktxParams.noSelectorRDO = false;

	return compress(ktxParams);
}

IBLLib::Result IBLLib::KtxImage::save(const char* _pathOut)
{	
	if (m_pTexture == nullptr)
	{
		return InvalidArgument;
	}

	FILE* outFile = fopen(_pathOut, "wb");

	if (outFile == nullptr)
	{
		printf("ktx: failed to open file %s\n", _pathOut);
		return FileNotFound;
	}

	KTX_error_code result = ktxTexture_WriteToStdioStream(m_pTexture, outFile);
	fclose(outFile);

	if (result != KTX_SUCCESS)
	{
		printf("ktx: %s\n", ktxErrorString(result));
		return KtxError;
	}
	
	return Success;
}

IBLLib::KtxImage::~KtxImage()
{
	if (m_pTexture != nullptr)
	{
		ktxTexture_Destroy(m_pTexture);
		m_pTexture = nullptr;
	}
}
