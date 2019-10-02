#include "lib.h"
#include <cstring>
#include <stdio.h>
#include <stdlib.h> 

using namespace IBLLib;

int main(int argc, char* argv[])
{
	const char* pathIn = "panorama.hdr";
	const char* pathOutSpecular = nullptr;
	const char* pathOutDiffuse = nullptr;
	unsigned int sampleCount = 1024u;
	unsigned int mipLevelCount = 10u;
	unsigned int cubeMapResolution = 1024u;
	unsigned int ktxVersion = 1u;
	unsigned int compressionQuality = 0u;
	OutputFormat targetFormat = R16G16B16A16_SFLOAT;
	float lodBias = 1.0f;
	bool inputIsCubeMap = false;
	bool enableDebugOutput = false;

	for (int i = 1; i+1 < argc; i += 2)
	{
		if (strcmp(argv[i], "-inputPath") == 0)
		{
			pathIn = argv[i + 1];
			printf("inputPath set to %s \n", pathIn);
		}
		else if (strcmp(argv[i], "-specularOutput") == 0)
		{
			pathOutSpecular = argv[i + 1];
			printf("specularOutput set to %s \n", pathOutSpecular);
		}
		else if (strcmp(argv[i], "-diffuseOutput") == 0)
		{
			pathOutDiffuse = argv[i + 1];
			printf("diffuseOutput set to %s \n", pathOutDiffuse);
		}
		else if (strcmp(argv[i], "-sampleCount") == 0)
		{
			sampleCount = strtoul(argv[i + 1], NULL, 0);
			printf("sampleCount set to %d \n", sampleCount);
		}
		else if (strcmp(argv[i], "-mipLevelCount") == 0)
		{
			mipLevelCount = strtoul(argv[i + 1], NULL, 0);
			printf("mipLevelCount set to %d \n", mipLevelCount);
		}
		else if (strcmp(argv[i], "-cubeMapResolution") == 0)
		{
			cubeMapResolution = strtoul(argv[i + 1], NULL, 0);
			printf("cubeMapResolution set to %d \n", cubeMapResolution);
		}
		else if (strcmp(argv[i], "-ktxVersion") == 0)
		{
			ktxVersion = strtoul(argv[i + 1], NULL, 0);
			printf("ktxVersion set to %d \n", ktxVersion);
		}
		else if (strcmp(argv[i], "-compressionQuality") == 0)
		{
			compressionQuality = strtoul(argv[i + 1], NULL, 0);
			printf("compressionQuality set to %d \n", compressionQuality);
		}
		else if (strcmp(argv[i], "-targetFormat") == 0)
		{
			const char* targetFormatString = argv[i + 1];

			if (strcmp(targetFormatString, "R8G8B8A8_UNORM") == 0)
			{
				targetFormat = R8G8B8A8_UNORM;
				printf("targetFormat set to R8G8B8A8_UNORM \n");
			}
			if (strcmp(targetFormatString, "R16G16B16A16_SFLOAT") == 0)
			{
				targetFormat = R16G16B16A16_SFLOAT;
				printf("targetFormat set to R16G16B16A16_SFLOAT \n");
			}
			if (strcmp(targetFormatString, "R32G32B32A32_SFLOAT") == 0)
			{
				targetFormat = R32G32B32A32_SFLOAT;
				printf("targetFormat set to R32G32B32A32_SFLOAT \n");
			}
		}
		else if (strcmp(argv[i], "-lodBias") == 0)
		{
			lodBias = atof(argv[i + 1]);
			printf("lodBias set to %f \n", lodBias);
		}
		else if (strcmp(argv[i], "-inputIsCubeMap") == 0)
		{
			inputIsCubeMap = true;
			printf("inputIsCubeMap flag is set.\n");
		}
		else if (strcmp(argv[i], "-debug") == 0)
		{
			enableDebugOutput = true;
			printf("debug flag is set.\n");
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-help") == 0)
		{
			printf("glTFIBLSampler\n");

			printf("-inputPath: path to panorama image (default) or cube map (if inputIsCubeMap flag ist set) \n");
			printf("-specularOutput: output path for specular term of filtered cube map\n");
			printf("-diffuseOutput: output path for diffuse term of filtered cube map\n");
			printf("-sampleCount: number of samples used for filtering (default = 1024)\n");
			printf("-mipLevelCount: number of mip levels of specular cube map\n");
			printf("-cubeMapResolution: resolution of output cube map (default = 1024)\n");
			printf("-ktxVersion: version 1 or version 2\n");
			printf("-compressionQuality: compression level for KTX2 files in range 0 - 255, 0 -> no compression \n");
			printf("-targetFormat: specify output texture format (R8G8B8A8_UNORM, R16G16B16A16_SFLOAT, R32G32B32A32_SFLOAT)  \n");
			printf("-lodBias: level of detail bias applied to filtering (default = 1 ) \n");
			printf("-inputIsCubeMap: if set, a cube map in ktx1 format is expected at input path \n");

			printf("\n");
		}
	}



	if (pathIn == nullptr) 
	{
		printf("Input path not set. Set input path with -inputPath.");
		return -1;
	}

	if (pathOutSpecular == nullptr)
	{
		if (ktxVersion == 1)
		{
			pathOutSpecular = "outputSpecular.ktx";
		}
		else
		{
			pathOutSpecular = "outputSpecular.ktx2";
		}
	}

	if (pathOutDiffuse == nullptr)
	{
		if (ktxVersion == 1)
		{
			pathOutDiffuse = "outputDiffuse.ktx";
		}
		else
		{
			pathOutDiffuse = "outputDiffuse.ktx2";
		}
	}

	if (ktxVersion == 1 && compressionQuality > 0)
	{
		printf("Compression not available for KTX1 files\n");
		printf("Set compression to 0 or use KTX2 file version\n");
		return -1;
	}

	Result res = sample(pathIn, pathOutSpecular, pathOutDiffuse, ktxVersion, compressionQuality, cubeMapResolution, mipLevelCount, sampleCount, targetFormat, lodBias, inputIsCubeMap, enableDebugOutput);

	if (res != Result::Success)
	{
		return -1;
	}

	return 0;
}