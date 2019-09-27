#pragma once
namespace IBLLib
{
	enum Result
	{
		Success = 0,
		VulkanInitializationFailed,
		VulkanError, // TODO: add more specific errors
		InputPanoramaFileNotFound,
		ShaderFileNotFound,
		ShaderCompilationFailed,
		FileNotFound,
		InvalidArgument,
		KtxError,
		StbError
	};
} // !IBLLib