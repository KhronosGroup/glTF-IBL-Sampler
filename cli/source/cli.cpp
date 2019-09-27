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

	for (int i = 1; i+1 < argc; i += 2)
	{
		if (strcmp(argv[i], "-in") == 0)
		{
			pathIn = argv[i + 1];
		}
		else if (strcmp(argv[i], "-specular") == 0)
		{
			pathOutSpecular = argv[i + 1];
		}
		else if (strcmp(argv[i], "-diffuse") == 0)
		{
			pathOutDiffuse = argv[i + 1];
		}
		else if (strcmp(argv[i], "-sampleCount") == 0)
		{
			sampleCount = strtoul(argv[i + 1], NULL, 0);
		}
		else if (strcmp(argv[i], "-mipLevelCount") == 0)
		{
			mipLevelCount = strtoul(argv[i + 1], NULL, 0);
		}
		else if (strcmp(argv[i], "-cubeMapResolution") == 0)
		{
			cubeMapResolution = strtoul(argv[i + 1], NULL, 0);
		}
		else if (strcmp(argv[i], "-ktxVersion") == 0)
		{
			ktxVersion = strtoul(argv[i + 1], NULL, 0);
		}
		else if (strcmp(argv[i], "-compressionQuality") == 0)
		{
			compressionQuality = strtoul(argv[i + 1], NULL, 0);
		}
		else if (strcmp(argv[i], "-targetFormat") == 0)
		{
			const char* targetFormatString = argv[i + 1];

			if (strcmp(targetFormatString, "R8G8B8A8_UNORM") == 0)
			{
				targetFormat = R8G8B8A8_UNORM;
			}
			if (strcmp(targetFormatString, "R16G16B16A16_SFLOAT") == 0)
			{
				targetFormat = R16G16B16A16_SFLOAT;
			}
			if (strcmp(targetFormatString, "R32G32B32A32_SFLOAT") == 0)
			{
				targetFormat = R32G32B32A32_SFLOAT;
			}
		}
		else if (strcmp(argv[i], "-h") == 0)
		{
			printf("glTFIBLSampler\n");
			printf("-specular: output path for specular term of filtered cube map\n");
			printf("-diffuse: output path for diffuse term of filtered cube map\n");
			printf("-sampleCount: number of samples used for filtering (default = 1024)\n");
			printf("-mipLevelCount: number of mip levels of specular cube map\n");
			printf("-cubeMapResolution: resolution of output cube map (default = 1024)\n");
			printf("-ktxVersion: version 1 or version 2\n");
			printf("-compressionQuality: compression level for KTX2 files in range 0 - 255, 0 -> no compression \n");
			printf("-targetFormat: specify output texture format (R8G8B8A8_UNORM, R16G16B16A16_SFLOAT, R32G32B32A32_SFLOAT)  \n");
		}
	}

	if (pathIn == nullptr) 
	{
		printf("Input path not set. Set input path with -in.");
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

	Result res = sample(pathIn, pathOutSpecular, pathOutDiffuse, ktxVersion, compressionQuality, cubeMapResolution, mipLevelCount, sampleCount, targetFormat); //pathin, pathout, resolution, mipmaps, sampleCount

	if (res != Result::Success)
	{
		return -1;
	}

	return 0;
}