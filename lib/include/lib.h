#pragma once
#include "ResultType.h"

namespace IBLLib
{
	enum OutputFormat
	{
		R8G8B8A8_UNORM = 37,
		R16G16B16A16_SFLOAT = 97,
		R32G32B32A32_SFLOAT = 109
	};

	enum Distribution : unsigned int 
	{
		Lambertian = 0,
		GGX = 1,
		Charlie = 2
	};

	Result sample(const char* _inputPath, const char* _outputPathCubeMap, const char* _outputPathLUT, Distribution _distribution, unsigned int  _cubemapResolution, unsigned int _mipmapCount, unsigned int _sampleCount, OutputFormat _targetFormat, float _lodBias, bool _inputIsCubeMap, bool _debugOutput);
} // !IBLLib