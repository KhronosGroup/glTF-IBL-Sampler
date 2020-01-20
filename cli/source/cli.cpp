#include "lib.h"
#include <cstring>
#include <stdio.h>
#include <stdlib.h> 

using namespace IBLLib;

int main(int argc, char* argv[])
{
	const char* pathIn = nullptr;
	const char* pathOutCubeMap = nullptr;
	const char* pathOutLUT = nullptr;
	unsigned int sampleCount = 1024u;
	unsigned int mipLevelCount = 10u;
	unsigned int cubeMapResolution = 1024u;
	OutputFormat targetFormat = R16G16B16A16_SFLOAT;
	Distribution distribution = GGX;
	float lodBias = 1.0f;
	bool inputIsCubeMap = false;
	bool enableDebugOutput = false;

	const char* targetFormatString = "R16G16B16A16_SFLOAT";
	const char* distributionString = "GGX";

	if (argc == 1 ||
		strcmp(argv[1], "-h") == 0 ||
		strcmp(argv[1], "-help") == 0)
	{
		printf("glTF-IBL-Sampler usage:\n");

		printf("-inputPath: path to panorama image (default) or cube map (if inputIsCubeMap flag ist set) \n");
		printf("-outCubeMap: output path for filtered cube map\n");
		printf("-outLUT output path for BRDF LUT\n");
		printf("-distribution NDF to sample (Lambertian, GGX, Charlie)\n");
		printf("-sampleCount: number of samples used for filtering (default = 1024)\n");
		printf("-mipLevelCount: number of mip levels of specular cube map\n");
		printf("-cubeMapResolution: resolution of output cube map (default = 1024)\n");
		printf("-targetFormat: specify output texture format (R8G8B8A8_UNORM, R16G16B16A16_SFLOAT, R32G32B32A32_SFLOAT)  \n");
		printf("-lodBias: level of detail bias applied to filtering (default = 1) \n");
		printf("-inputIsCubeMap: if set, a cube map in ktx1 format is expected at input path \n");

		return 0;
	}

	for (int i = 1; i < argc; ++i)
	{
		const char* nextArg = i + 1 < argc ? argv[i+1] : nullptr;
		if (strcmp(argv[i], "-inputPath") == 0)
		{
			pathIn = nextArg;
		}
		else if (strcmp(argv[i], "-outCubeMap") == 0)
		{
			pathOutCubeMap = nextArg;
		}
		else if (strcmp(argv[i], "-outLUT") == 0)
		{
			pathOutLUT = nextArg;
		}
		else if (strcmp(argv[i], "-sampleCount") == 0)
		{
			sampleCount = strtoul(nextArg, NULL, 0);
		}
		else if (strcmp(argv[i], "-mipLevelCount") == 0)
		{
			mipLevelCount = strtoul(nextArg, NULL, 0);
		}
		else if (strcmp(argv[i], "-cubeMapResolution") == 0)
		{
			cubeMapResolution = strtoul(nextArg, NULL, 0);
		}
		else if (strcmp(argv[i], "-targetFormat") == 0)
		{
			 targetFormatString = nextArg;

			if (strcmp(targetFormatString, "R8G8B8A8_UNORM") == 0)
			{
				targetFormat = R8G8B8A8_UNORM;
			}
			else if (strcmp(targetFormatString, "R16G16B16A16_SFLOAT") == 0)
			{
				targetFormat = R16G16B16A16_SFLOAT;
			}
			else if (strcmp(targetFormatString, "R32G32B32A32_SFLOAT") == 0)
			{
				targetFormat = R32G32B32A32_SFLOAT;
			}
		}
		else if (strcmp(argv[i], "-distribution") == 0)
		{
			distributionString = nextArg;

			if (strcmp(distributionString, "Lambertian") == 0)
			{
				distribution = Lambertian;
			}
			else if (strcmp(distributionString, "GGX") == 0)
			{
				distribution = GGX;
			}
			else if (strcmp(distributionString, "Charlie") == 0)
			{
				distribution = Charlie;
			}
		}
		else if (strcmp(argv[i], "-lodBias") == 0)
		{
			lodBias = atof(nextArg);
		}
		else if (strcmp(argv[i], "-inputIsCubeMap") == 0)
		{
			inputIsCubeMap = true;
		}
		else if (strcmp(argv[i], "-debug") == 0)
		{
			enableDebugOutput = true;
		}
	}

	if (argc == 2)
	{
		pathIn = argv[1];
	}

	if (pathIn == nullptr) 
	{
		printf("Input path not set. Set input path with -inputPath.\n");
		return -1;
	}

	if (pathOutCubeMap == nullptr)
	{
		pathOutCubeMap = "outputCubeMap.ktx2";
	}

	if (pathOutLUT == nullptr)
	{
		pathOutLUT = "outputLUT.png";
	}

	printf("inputPath set to %s \n", pathIn);
	printf("outCubeMap set to %s \n", pathOutCubeMap);

	if (pathOutLUT != nullptr)
	{
		printf("outLUT set to %s \n", pathOutLUT);	
	}

	printf("sampleCount set to %d \n", sampleCount);
	printf("mipLevelCount set to %d \n", mipLevelCount);
	printf("targetFormat set to %s\n", targetFormatString);
	printf("distribution set to %s\n", distributionString);
	printf("lodBias set to %f \n", lodBias);
	printf("inputIsCubeMap flag is set to %s\n", inputIsCubeMap ? "True" : "False");
	printf("debug flag is set to %s\n", enableDebugOutput ? "True" : "False");

	Result res = sample(pathIn, pathOutCubeMap, pathOutLUT, distribution, cubeMapResolution, mipLevelCount, sampleCount, targetFormat, lodBias, inputIsCubeMap, enableDebugOutput);

	if (res != Result::Success)
	{
		return -1;
	}

	return 0;
}
