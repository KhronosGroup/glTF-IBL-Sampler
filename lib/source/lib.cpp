#include "lib.h"
#include "vkHelper.h"
#include "ShaderCompiler.h"
#include "STBImage.h"
#include "FileHelper.h"
#include "KtxImage.h"
#include <vk_format_utils.h>
#include "formatHelper.h"

namespace IBLLib
{
	Result compileShader(vkHelper& _vulkan, const char* _path, const char* _entryPoint, VkShaderModule& _outModule, ShaderCompiler::Stage _stage)
	{
		std::vector<char>  glslBuffer;
		std::vector<uint32_t> outSpvBlob;

		if (readFile(_path, glslBuffer) == false)
		{
			return Result::ShaderFileNotFound;
		}

		if (ShaderCompiler::instance().compile(glslBuffer, _entryPoint, _stage, outSpvBlob) == false)
		{
			return Result::ShaderCompilationFailed;
		}

		if (_vulkan.loadShaderModule(_outModule, outSpvBlob.data(), outSpvBlob.size() * 4) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		return Result::Success;
	}

	Result uploadImage(vkHelper& _vulkan, const char* _inputPath, VkImage& _outImage)
	{
		_outImage = VK_NULL_HANDLE;
		STBImage panorama;

		if (panorama.loadHdr(_inputPath) != Result::Success)
		{
			return Result::InputPanoramaFileNotFound;
		}

		VkCommandBuffer uploadCmds = VK_NULL_HANDLE;
		if (_vulkan.createCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// create staging buffer for image data
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		if (_vulkan.createBufferAndAllocate(stagingBuffer, static_cast<uint32_t>(panorama.getByteSize()), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// transfer data to the host coherent staging buffer 
		if (_vulkan.writeBufferData(stagingBuffer, panorama.getHdrData(), panorama.getByteSize()) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// create the destination image we want to sample in the shader
		if (_vulkan.createImage2DAndAllocate(_outImage, panorama.getWidth(), panorama.getHeight(), VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.beginCommandBuffer(uploadCmds, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// transition to write dst layout
		_vulkan.transitionImageToTransferWrite(uploadCmds, _outImage);
		_vulkan.copyBufferToBasicImage2D(uploadCmds, stagingBuffer, _outImage);
		_vulkan.transitionImageToShaderRead(uploadCmds, _outImage);

		if (_vulkan.endCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.executeCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		_vulkan.destroyBuffer(stagingBuffer);
		_vulkan.destroyCommandBuffer(uploadCmds);

		return Result::Success;
	}

	Result uploadKtxImage(vkHelper& _vulkan, const char* _inputPath, VkImage& _outImage, uint32_t _requestedMipLevels = 1)
	{
		_outImage = VK_NULL_HANDLE;
		Result result = Result::Success;
	
		KtxImage ktxImage;
		result = ktxImage.loadKtx1(_inputPath);
		if (result != Success)
		{
			return Result::KtxError;
		}
	
		ktxTexture1* textureInformation = ktxImage.getTexture1();
		if(textureInformation == nullptr)
		{
			printf("Error: failed to get texture information\n");
			return KtxError;
		}

		const uint32_t dataByteSize = textureInformation->dataSize;
		const uint32_t width = textureInformation->baseWidth;
		const uint32_t height = textureInformation->baseHeight;
		const uint32_t faces = textureInformation->isCubemap ? 6 : 1;
		const VkFormat vkFormat = IBLLib::glToVulkanFormat(textureInformation->glInternalformat);
		const uint32_t formatSize = FormatElementSize(vkFormat);
		
		if(textureInformation->numLevels>1)
		{
			printf("Error: unexpected mip levels\n");
			return InvalidArgument;
		}

		if(vkFormat == VK_FORMAT_UNDEFINED)
		{
			printf("Error: VkFormat of ktx file not supported\n");
			return InvalidArgument;
		}

		if (textureInformation->isCubemap == false)
		{
			printf("Error: ktx file does not contain a cubemap\n");
			return InvalidArgument;
		}

		const uint32_t expectedSize = width * height * faces * formatSize;

		if (dataByteSize != expectedSize)
		{
			printf("Error: input size mismatch\n");
			return InvalidArgument;
		}

		const uint8_t* data = ktxImage.getData();
		
		if(data == nullptr)
		{
			printf("Error: ktx data is nullptr\n");
			return Result::KtxError;
		}

		printf("Uploading data to device\n");

		VkCommandBuffer uploadCmds = VK_NULL_HANDLE;
		if (_vulkan.createCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// create staging buffer for image data
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		if (_vulkan.createBufferAndAllocate(stagingBuffer, static_cast<uint32_t>(dataByteSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != VK_SUCCESS)
		{
			printf("Error: failed creating buffer\n");
			return Result::VulkanError;
		}

		// transfer data to the host coherent staging buffer 
		if (_vulkan.writeBufferData(stagingBuffer, data, dataByteSize) != VK_SUCCESS)
		{
			printf("Error: failed writing to buffer\n");
			return Result::VulkanError;
		}

		if (_vulkan.createImage2DAndAllocate(_outImage, width, height, vkFormat,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			_requestedMipLevels, 6u, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.beginCommandBuffer(uploadCmds, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// transition to write dst layout

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = faces;

		_vulkan.imageBarrier(uploadCmds, _outImage,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0u,//src stage, access
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,//dst stage, access
			subresourceRange);
		
		_vulkan.copyBufferToBasicImage2D(uploadCmds, stagingBuffer, _outImage);

		_vulkan.imageBarrier(uploadCmds, _outImage,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,//src stage, access
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,//dst stage, access
			subresourceRange);


		if (_vulkan.endCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.executeCommandBuffer(uploadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		_vulkan.destroyBuffer(stagingBuffer);
		_vulkan.destroyCommandBuffer(uploadCmds);

		return Result::Success;
	}

	Result convertVkFormat(vkHelper& _vulkan, const VkCommandBuffer _commandBuffer, const VkImage _srcImage, VkImage& _outImage, VkFormat _dstFormat, const VkImageLayout inputImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		const VkImageCreateInfo* pInfo = _vulkan.getCreateInfo(_srcImage);

		if (pInfo == nullptr)
		{
			return Result::InvalidArgument;
		}

		if (_outImage != VK_NULL_HANDLE)
		{
			printf("Expecting empty outImage\n");
			return Result::InvalidArgument;
		}

		const VkFormat srcFormat = pInfo->format; 
		const uint32_t sideLength = pInfo->extent.width;
		const uint32_t mipLevels = pInfo->mipLevels;
		const uint32_t arrayLayers = pInfo->arrayLayers;

		if (_vulkan.createImage2DAndAllocate(_outImage, sideLength, sideLength, _dstFormat,
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			mipLevels, arrayLayers, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = arrayLayers;
	
		_vulkan.imageBarrier(_commandBuffer, _outImage,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,//src stage, access
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,//dst stage, access
			subresourceRange);

		_vulkan.imageBarrier(_commandBuffer, _srcImage,
			inputImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,//src stage, access
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,//dst stage, access
			subresourceRange);
	
		uint32_t currentSideLength = sideLength;

		for (uint32_t level = 0; level < mipLevels; level++)
		{
			VkImageBlit imageBlit{};

			// Source
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = arrayLayers;
			imageBlit.srcSubresource.mipLevel = level;
			imageBlit.srcOffsets[1].x = currentSideLength;
			imageBlit.srcOffsets[1].y = currentSideLength;
			imageBlit.srcOffsets[1].z = 1;

			// Destination
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = arrayLayers;
			imageBlit.dstSubresource.mipLevel = level;
			imageBlit.dstOffsets[1].x = currentSideLength;
			imageBlit.dstOffsets[1].y = currentSideLength;
			imageBlit.dstOffsets[1].z = 1;

			vkCmdBlitImage(
				_commandBuffer,
				_srcImage,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				_outImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageBlit,
				VK_FILTER_LINEAR);

			currentSideLength = currentSideLength >> 1;
		}

		return Result::Success;
	}

	Result downloadCubemap(vkHelper& _vulkan, const VkImage _srcImage, const char* _outputPath, KtxImage::Version _ktxVersion, unsigned int _ktxCompressionQuality, const VkImageLayout inputImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		const VkImageCreateInfo* pInfo = _vulkan.getCreateInfo(_srcImage);
		if (pInfo == nullptr)
		{
			return Result::InvalidArgument;
		}

		Result res = Success;

		const VkFormat cubeMapFormat = pInfo->format; 
		const uint32_t cubeMapFormatByteSize = FormatElementSize(cubeMapFormat);
		const uint32_t cubeMapSideLength = pInfo->extent.width;
		const uint32_t mipLevels = pInfo->mipLevels;

		using Faces = std::vector<VkBuffer>;
		using MipLevels = std::vector<Faces>;

		MipLevels stagingBuffer(mipLevels);

		{
			uint32_t currentSideLength = cubeMapSideLength;

			for (uint32_t level = 0; level < mipLevels; level++)
			{
				Faces& faces = stagingBuffer[level];
				faces.resize(6u);

				for (uint32_t face = 0; face < 6u; face++)
				{
					if (_vulkan.createBufferAndAllocate(
						faces[face], currentSideLength * currentSideLength * cubeMapFormatByteSize,
						VK_BUFFER_USAGE_TRANSFER_DST_BIT,// VkBufferUsageFlags _usage,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)//VkMemoryPropertyFlags _memoryFlags, 
						!= VK_SUCCESS)
					{
						return Result::VulkanError;
					}
				}

				currentSideLength = currentSideLength >> 1;
			}
		}

		VkCommandBuffer downloadCmds = VK_NULL_HANDLE;
		if (_vulkan.createCommandBuffer(downloadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.beginCommandBuffer(downloadCmds, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		// barrier on complete image
		VkImageSubresourceRange  subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseArrayLayer = 0u;
		subresourceRange.layerCount = 6u;
		subresourceRange.baseMipLevel = 0u;
		subresourceRange.levelCount = mipLevels;

		_vulkan.imageBarrier(downloadCmds, _srcImage,
			inputImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // src stage, access
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			subresourceRange);//dst stage, access

		// copy all faces & levels into staging buffers
		{
			uint32_t currentSideLength = cubeMapSideLength;

			VkBufferImageCopy region{};

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1u;

			for (uint32_t level = 0; level < mipLevels; level++)
			{
				region.imageSubresource.mipLevel = level;
				Faces& faces = stagingBuffer[level];

				for (uint32_t face = 0; face < 6u; face++)
				{
					region.imageSubresource.baseArrayLayer = face;
					region.imageExtent = { currentSideLength , currentSideLength , 1u };

					_vulkan.copyImage2DToBuffer(downloadCmds, _srcImage, faces[face], region);
				}

				currentSideLength = currentSideLength >> 1;
			}
		}

		if (_vulkan.endCommandBuffer(downloadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		if (_vulkan.executeCommandBuffer(downloadCmds) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
		
		_vulkan.destroyCommandBuffer(downloadCmds);

		// Image is copied to buffer
		// Now map buffer and copy to ram
		{
			KtxImage ktxImage(_ktxVersion, cubeMapSideLength, cubeMapSideLength, cubeMapFormat, mipLevels, true);

			std::vector<unsigned char> imageData;

			uint32_t currentSideLength = cubeMapSideLength;

			for (uint32_t level = 0; level < mipLevels; level++)
			{
				const size_t imageByteSize = (size_t)currentSideLength * (size_t)currentSideLength * (size_t)cubeMapFormatByteSize;
				imageData.resize(imageByteSize);

				Faces& faces = stagingBuffer[level];

				for (uint32_t face = 0; face < 6u; face++)
				{
					if (_vulkan.readBufferData(faces[face], imageData.data(), imageByteSize) != VK_SUCCESS)
					{
						return Result::VulkanError;
					}

					res = ktxImage.writeFace(imageData, face, level);
					if (res != Result::Success)
					{
						return res;
					}

					_vulkan.destroyBuffer(faces[face]);
				}

				currentSideLength = currentSideLength >> 1;
			}

			if (_ktxCompressionQuality > 0)
			{
				res = ktxImage.compress(_ktxCompressionQuality);
				if (res != Result::Success)
				{
					printf("Compression failed\n");
					return res;
				}
			}

			res = ktxImage.save(_outputPath);
			if (res != Result::Success)
			{
				return res;
			}
		}

		return Result::Success;
	}

	void generateMipmapLevels(vkHelper& _vulkan, const VkCommandBuffer _commandBuffer, const VkImage _image, uint32_t _maxMipLevels, uint32_t _sideLength, const VkImageLayout _currentImageLayout)
	{
		{ 
			VkImageSubresourceRange mipbaseRange{};
			mipbaseRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipbaseRange.baseMipLevel = 0u;
			mipbaseRange.levelCount = 1u;
			mipbaseRange.layerCount = 6u;

			_vulkan.imageBarrier(_commandBuffer, _image,
				_currentImageLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,//dst stage, access
				mipbaseRange);
		}

		for (uint32_t i = 1; i < _maxMipLevels; i++)
		{
			VkImageBlit imageBlit{};

			// Source
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = 6u;
			imageBlit.srcSubresource.mipLevel = i - 1;
			imageBlit.srcOffsets[1].x = int32_t(_sideLength >> (i - 1));
			imageBlit.srcOffsets[1].y = int32_t(_sideLength >> (i - 1));
			imageBlit.srcOffsets[1].z = 1;
			
			// Destination
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = 6u;
			imageBlit.dstSubresource.mipLevel = i;
			imageBlit.dstOffsets[1].x = int32_t(_sideLength >> i);
			imageBlit.dstOffsets[1].y = int32_t(_sideLength >> i);
			imageBlit.dstOffsets[1].z = 1;

			VkImageSubresourceRange mipSubRange = {};
			mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipSubRange.baseMipLevel = i;
			mipSubRange.levelCount = 1;
			mipSubRange.layerCount = 6u;

			//  Transiton current mip level to transfer dest			
			_vulkan.imageBarrier(_commandBuffer, _image,
				_currentImageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				mipSubRange);//dst stage, access

			vkCmdBlitImage(
				_commandBuffer,
				_image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				_image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&imageBlit,
				VK_FILTER_LINEAR);


			//  Transiton  back
			_vulkan.imageBarrier(_commandBuffer, _image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,//dst stage, access
				mipSubRange);

		}
		
		{
			VkImageSubresourceRange completeRange{};
			completeRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			completeRange.baseMipLevel = 0;
			completeRange.levelCount = _maxMipLevels;
			completeRange.layerCount = 6u;

			_vulkan.imageBarrier(_commandBuffer, _image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,//dst stage, access
				completeRange);
		} 
	}

	Result panoramaToCubemap(vkHelper& _vulkan, const VkCommandBuffer _commandBuffer, const VkRenderPass _renderPass, const VkShaderModule fullscreenVertexShader, const VkImage _panoramaImage, const VkImage _cubeMapImage)
	{
		IBLLib::Result res = Result::Success;

		const VkImageCreateInfo* textureInfo = _vulkan.getCreateInfo(_cubeMapImage);
		
		if (textureInfo == nullptr)
		{
			return Result::InvalidArgument;
		}

		const uint32_t cubeMapSideLength = textureInfo->extent.width;
		const uint32_t maxMipLevels = textureInfo->mipLevels;

		VkShaderModule panoramaToCubeMapFragmentShader = VK_NULL_HANDLE;
		if ((res = compileShader(_vulkan, IBLSAMPLER_SHADERS_DIR  "/filter.frag", "panoramaToCubeMap", panoramaToCubeMapFragmentShader, ShaderCompiler::Stage::Fragment)) != Result::Success)
		{
			return res;
		}

		VkSamplerCreateInfo samplerInfo{};
		_vulkan.fillSamplerCreateInfo(samplerInfo);

		samplerInfo.maxLod = float(maxMipLevels + 1);
		VkSampler panoramaSampler = VK_NULL_HANDLE;
		if (_vulkan.createSampler(panoramaSampler, samplerInfo) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		VkImageView panoramaImageView = VK_NULL_HANDLE;
		if (_vulkan.createImageView(panoramaImageView, _panoramaImage) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		VkPipelineLayout panoramaPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSet panoramaSet = VK_NULL_HANDLE;
		VkPipeline panoramaToCubeMapPipeline = VK_NULL_HANDLE;
		{
			//
			// Create pipeline layout
			//
			VkDescriptorSetLayout panoramaSetLayout = VK_NULL_HANDLE;
			DescriptorSetInfo setLayout0;
			setLayout0.addCombinedImageSampler(panoramaSampler, panoramaImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			if (setLayout0.create(_vulkan, panoramaSetLayout, panoramaSet) != VK_SUCCESS)
			{
				return Result::VulkanError;
			}

			_vulkan.updateDescriptorSets(setLayout0.getWrites());

			if (_vulkan.createPipelineLayout(panoramaPipelineLayout, panoramaSetLayout) != VK_SUCCESS)
			{
				return Result::VulkanError;
			}

			GraphicsPipelineDesc panormaToCubePipeline;

			panormaToCubePipeline.addShaderStage(fullscreenVertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main");
			panormaToCubePipeline.addShaderStage(panoramaToCubeMapFragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, "panoramaToCubeMap");

			panormaToCubePipeline.setRenderPass(_renderPass);
			panormaToCubePipeline.setPipelineLayout(panoramaPipelineLayout);
						
			VkPipelineColorBlendAttachmentState colorBlendAttachment{};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;

			panormaToCubePipeline.addColorBlendAttachment(colorBlendAttachment,6); 

			panormaToCubePipeline.setViewportExtent(VkExtent2D{ cubeMapSideLength, cubeMapSideLength });

			if (_vulkan.createPipeline(panoramaToCubeMapPipeline, panormaToCubePipeline.getInfo()) != VK_SUCCESS)
			{
				return Result::VulkanError;
			}
		}

		///// Render Pass
		std::vector<VkImageView> inputCubeMapViews(6u, VK_NULL_HANDLE);
		for (size_t i = 0; i < inputCubeMapViews.size(); i++)
		{
			if (_vulkan.createImageView(inputCubeMapViews[i], _cubeMapImage, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, static_cast<uint32_t>(i), 1u }) != VK_SUCCESS)
			{
				return Result::VulkanError;
			}
		}

		VkFramebuffer cubeMapInputFramebuffer = VK_NULL_HANDLE;
		if (_vulkan.createFramebuffer(cubeMapInputFramebuffer, _renderPass, cubeMapSideLength, cubeMapSideLength, inputCubeMapViews, 1u) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		{
			VkImageSubresourceRange  subresourceRangeBaseMiplevel = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, maxMipLevels, 0u, 6u };

			_vulkan.imageBarrier(_commandBuffer, _cubeMapImage,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,// src stage, access	
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, //dst stage, access
				subresourceRangeBaseMiplevel);
		}

		_vulkan.bindDescriptorSet(_commandBuffer, panoramaPipelineLayout, panoramaSet);

		vkCmdBindPipeline(_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, panoramaToCubeMapPipeline);

		const std::vector<VkClearValue> clearValues(6u, { 0.0f, 0.0f, 1.0f, 1.0f });

		_vulkan.beginRenderPass(_commandBuffer, _renderPass, cubeMapInputFramebuffer, VkRect2D{ 0u, 0u, cubeMapSideLength, cubeMapSideLength }, clearValues);
		vkCmdDraw(_commandBuffer, 3, 1u, 0, 0);
		_vulkan.endRenderPass(_commandBuffer);

		return res;
	}
} // !IBLLib


IBLLib::Result IBLLib::sample(const char* _inputPath, const char* _outputPathSpecular, const char* _outputPathDiffuse, unsigned int _ktxVersion, unsigned int _ktxCompressionQuality, unsigned int _cubemapResolution, unsigned int _mipmapCount, unsigned int _sampleCount, OutputFormat _targetFormat, float _lodBias, bool _inputIsCubeMap, bool _debugOutput)
{
	const VkFormat cubeMapFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	const uint32_t cubeMapSideLength = _cubemapResolution;
	const uint32_t outputMipLevels = _mipmapCount;
	
	uint32_t maxMipLevels = 0u;
	for (uint32_t m = cubeMapSideLength; m > 0; m = m >> 1, ++maxMipLevels) {}

	if (_cubemapResolution >> _mipmapCount < 1)
	{
		printf("Error: CubemapResolution incompatible with MipmapCount ");
		return Result::InvalidArgument;
	}

	IBLLib::Result res = Result::Success;

	vkHelper vulkan;

	if (vulkan.initialize(0u, 1u, _debugOutput) != VK_SUCCESS)
	{
		return Result::VulkanInitializationFailed;
	}

	VkImage panoramaImage;
	if (_inputIsCubeMap == false)
	{
		if ((res = uploadImage(vulkan, _inputPath, panoramaImage)) != Result::Success)
		{
			return res;
		}
	}

	VkShaderModule fullscreenVertexShader = VK_NULL_HANDLE;
	if ((res = compileShader(vulkan, IBLSAMPLER_SHADERS_DIR "/primitive.vert", "main", fullscreenVertexShader, ShaderCompiler::Stage::Vertex)) != Result::Success)
	{
		return res;
	}

	VkShaderModule filterCubeMapSpecular = VK_NULL_HANDLE;
	if ((res = compileShader(vulkan, IBLSAMPLER_SHADERS_DIR "/filter.frag", "filterCubeMapSpecular", filterCubeMapSpecular, ShaderCompiler::Stage::Fragment)) != Result::Success)
	{
		return res;
	}

	VkShaderModule filterCubeMapDiffuse = VK_NULL_HANDLE;
	if ((res = compileShader(vulkan, IBLSAMPLER_SHADERS_DIR "/filter.frag", "filterCubeMapDiffuse", filterCubeMapDiffuse, ShaderCompiler::Stage::Fragment)) != Result::Success)
	{
		return res;
	}
	
	VkSampler cubeMipMapSampler = VK_NULL_HANDLE;
	{
		VkSamplerCreateInfo samplerInfo{};
		vulkan.fillSamplerCreateInfo(samplerInfo);
		samplerInfo.maxLod = float(maxMipLevels + 1);

		if (vulkan.createSampler(cubeMipMapSampler, samplerInfo) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}
	
	VkImage inputCubeMap = VK_NULL_HANDLE;
	VkImageLayout currentInputCubeMapLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if(_inputIsCubeMap)	
	{
		//Loading cubemap directly from ktx file
		uploadKtxImage(vulkan, _inputPath, inputCubeMap, maxMipLevels);
	}
	else
	{ 
		//VK_IMAGE_USAGE_TRANSFER_SRC_BIT needed for transfer to staging buffer
		if (vulkan.createImage2DAndAllocate(inputCubeMap, cubeMapSideLength, cubeMapSideLength, cubeMapFormat,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			maxMipLevels, 6u, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	VkImageView inputCubeMapCompleteView = VK_NULL_HANDLE;
	if (vulkan.createImageView(inputCubeMapCompleteView, inputCubeMap, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, maxMipLevels, 0u, 6u }, VK_FORMAT_UNDEFINED, VK_IMAGE_VIEW_TYPE_CUBE) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}
	
	VkImage outputSpecularCubeMap = VK_NULL_HANDLE;
	if (vulkan.createImage2DAndAllocate(outputSpecularCubeMap, cubeMapSideLength, cubeMapSideLength, cubeMapFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		outputMipLevels, 6u, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}

	std::vector< std::vector<VkImageView> > outputSpecularCubeMapViews(outputMipLevels);

	for (uint32_t i = 0; i < outputMipLevels; ++i)
	{
		outputSpecularCubeMapViews[i].resize(6, VK_NULL_HANDLE); //sides of the cube

		for (uint32_t j = 0; j < 6; j++)
		{
			VkImageSubresourceRange subresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u };
			subresourceRange.baseMipLevel = i;
			subresourceRange.baseArrayLayer = j;
			if (vulkan.createImageView(outputSpecularCubeMapViews[i][j], outputSpecularCubeMap, subresourceRange) != VK_SUCCESS)
			{
				return Result::VulkanError;
			}
		}
	}

	VkImageView outputSpecularCubeMapCompleteView = VK_NULL_HANDLE;
	{
		VkImageSubresourceRange subresourceRange{};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseArrayLayer = 0u;
		subresourceRange.baseMipLevel = 0u;
		subresourceRange.layerCount = 6u;
		subresourceRange.levelCount = outputMipLevels;

		if (vulkan.createImageView(outputSpecularCubeMapCompleteView, outputSpecularCubeMap, subresourceRange, VK_FORMAT_UNDEFINED, VK_IMAGE_VIEW_TYPE_CUBE) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	VkImage outputDiffuseCubeMap = VK_NULL_HANDLE;
	if (vulkan.createImage2DAndAllocate(outputDiffuseCubeMap, cubeMapSideLength, cubeMapSideLength, cubeMapFormat,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		1u, 6u, VK_IMAGE_TILING_OPTIMAL, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}

	std::vector<VkImageView> outputDiffuseCubeMapViews(6u, VK_NULL_HANDLE);
	for (size_t i = 0; i < outputDiffuseCubeMapViews.size(); i++)
	{
		if (vulkan.createImageView(outputDiffuseCubeMapViews[i], outputDiffuseCubeMap, { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, static_cast<uint32_t>(i), 1u }) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	VkRenderPass renderPass = VK_NULL_HANDLE;
	{
		RenderPassDesc renderPassDesc;

		// add rendertargets (cubemap faces)
		for (int face = 0; face < 6; ++face)
		{
			renderPassDesc.addAttachment(cubeMapFormat);
		}

		if (vulkan.createRenderPass(renderPass, renderPassDesc.getInfo()) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	//Push Constants for specular and diffuse filter passes  
	struct PushConstant
	{
		float roughness = 0.f;
		uint32_t sampleCount = 1u;
		uint32_t mipLevel = 1u;
		uint32_t width = 1024u;
		float lodBias = 0.f;
	};

	std::vector<VkPushConstantRange> ranges(1u);
	VkPushConstantRange& range = ranges.front();

	range.offset = 0u;
	range.size = sizeof(PushConstant);
	range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	////////////////////////////////////////////////////////////////////////////////////////
	// Specular Filter CubeMap
	VkDescriptorSet filterDescriptorSet = VK_NULL_HANDLE;
	VkPipelineLayout filterPipelineLayout = VK_NULL_HANDLE;
	VkPipeline specularFilterPipeline = VK_NULL_HANDLE;
	{
		DescriptorSetInfo setLayout0;
		uint32_t binding = 1u;
		setLayout0.addCombinedImageSampler(cubeMipMapSampler, inputCubeMapCompleteView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, binding, VK_SHADER_STAGE_FRAGMENT_BIT); // change sampler ?

		VkDescriptorSetLayout specularSetLayout = VK_NULL_HANDLE;
		if (setLayout0.create(vulkan, specularSetLayout, filterDescriptorSet) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		vulkan.updateDescriptorSets(setLayout0.getWrites());

		if (vulkan.createPipelineLayout(filterPipelineLayout, specularSetLayout, ranges) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		GraphicsPipelineDesc filterCubeMapPipelineDesc;

		filterCubeMapPipelineDesc.addShaderStage(fullscreenVertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main");
		filterCubeMapPipelineDesc.addShaderStage(filterCubeMapSpecular, VK_SHADER_STAGE_FRAGMENT_BIT, "filterCubeMapSpecular");

		filterCubeMapPipelineDesc.setRenderPass(renderPass);
		filterCubeMapPipelineDesc.setPipelineLayout(filterPipelineLayout);

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // TODO: rgb only
		colorBlendAttachment.blendEnable = VK_FALSE;

		filterCubeMapPipelineDesc.addColorBlendAttachment(colorBlendAttachment, 6u); 	

		filterCubeMapPipelineDesc.setViewportExtent(VkExtent2D{ cubeMapSideLength, cubeMapSideLength });

		if (vulkan.createPipeline(specularFilterPipeline, filterCubeMapPipelineDesc.getInfo()) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Diffuse Filter 
	VkPipeline diffuseFilterPipeline = VK_NULL_HANDLE;
	{
		GraphicsPipelineDesc diffuseFilterPipelineDesc;

		diffuseFilterPipelineDesc.addShaderStage(fullscreenVertexShader, VK_SHADER_STAGE_VERTEX_BIT, "main");
		diffuseFilterPipelineDesc.addShaderStage(filterCubeMapDiffuse, VK_SHADER_STAGE_FRAGMENT_BIT, "filterCubeMapDiffuse");

		diffuseFilterPipelineDesc.setRenderPass(renderPass);
		diffuseFilterPipelineDesc.setPipelineLayout(filterPipelineLayout);

		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		diffuseFilterPipelineDesc.addColorBlendAttachment(colorBlendAttachment, 6u);

		diffuseFilterPipelineDesc.setViewportExtent(VkExtent2D{ cubeMapSideLength, cubeMapSideLength });

		if (vulkan.createPipeline(diffuseFilterPipeline, diffuseFilterPipelineDesc.getInfo()) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}
	}

	const std::vector<VkClearValue> clearValues(6u, { 0.0f, 0.0f, 1.0f, 1.0f });

	VkCommandBuffer cubeMapCmd;
	if (vulkan.createCommandBuffer(cubeMapCmd) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}

	if (vulkan.beginCommandBuffer(cubeMapCmd, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}

	////////////////////////////////////////////////////////////////////////////////////////
	// Transform panorama image to cube map
	if (_inputIsCubeMap == false)
	{
		printf("Transform panorama image to cube map\n");

		res = panoramaToCubemap(vulkan, cubeMapCmd, renderPass, fullscreenVertexShader, panoramaImage, inputCubeMap);
		if (res != VK_SUCCESS)
		{
			printf("Failed to transform panorama image to cube map\n");
			return res;
		}

		currentInputCubeMapLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	////////////////////////////////////////////////////////////////////////////////////////
	//Generate MipLevels
	printf("Generating mipmap levels\n");
	generateMipmapLevels(vulkan, cubeMapCmd, inputCubeMap, maxMipLevels, cubeMapSideLength, currentInputCubeMapLayout);
	currentInputCubeMapLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Filter specular
	printf("Filtering specular term\n");
	vulkan.bindDescriptorSet(cubeMapCmd, filterPipelineLayout, filterDescriptorSet);

	vkCmdBindPipeline(cubeMapCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, specularFilterPipeline);

	unsigned int currentFramebufferSideLength = cubeMapSideLength;

	//Filter every mip level: from inputCubeMap->currentMipLevel
	for (uint32_t currentMipLevel = 0; currentMipLevel < outputMipLevels; currentMipLevel++)
	{
		//Framebuffer will be destroyed automatically at shutdown
		VkFramebuffer cubeMapOutputFramebuffer = VK_NULL_HANDLE;
		if (vulkan.createFramebuffer(cubeMapOutputFramebuffer, renderPass, currentFramebufferSideLength, currentFramebufferSideLength, outputSpecularCubeMapViews[currentMipLevel], 1u) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		VkImageSubresourceRange  subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, currentMipLevel, 1u, 0u, 6u };

		vulkan.imageBarrier(cubeMapCmd, outputSpecularCubeMap,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,//src stage, access
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dst stage, access		
			subresourceRange);

		PushConstant values{};
		values.roughness = currentMipLevel / static_cast<float>(outputMipLevels);
		values.sampleCount = _sampleCount;
		values.mipLevel = currentMipLevel;
		values.width = cubeMapSideLength;
		values.lodBias = _lodBias;

		vkCmdPushConstants(cubeMapCmd, filterPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &values);
	
		vulkan.beginRenderPass(cubeMapCmd, renderPass, cubeMapOutputFramebuffer, VkRect2D{ 0u, 0u, currentFramebufferSideLength, currentFramebufferSideLength }, clearValues);
		vkCmdDraw(cubeMapCmd, 3, 1u, 0, 0);
		vulkan.endRenderPass(cubeMapCmd);		 

		currentFramebufferSideLength = currentFramebufferSideLength >> 1;
	}

	// Filter diffuse
	printf("Filtering diffuse term\n");
	{
		VkFramebuffer diffuseCubeMapFramebuffer = VK_NULL_HANDLE;
		if (vulkan.createFramebuffer(diffuseCubeMapFramebuffer, renderPass, cubeMapSideLength, cubeMapSideLength, outputDiffuseCubeMapViews, 1u) != VK_SUCCESS)
		{
			return Result::VulkanError;
		}

		VkImageSubresourceRange  subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1u, 0u, 6u };

		vulkan.imageBarrier(cubeMapCmd, outputDiffuseCubeMap,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,//src stage, access
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dst stage, access		
			subresourceRange);

		vulkan.bindDescriptorSet(cubeMapCmd, filterPipelineLayout, filterDescriptorSet);

		vkCmdBindPipeline(cubeMapCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, diffuseFilterPipeline);
		
		PushConstant values;
		values.roughness = 0;
		values.sampleCount = _sampleCount;
		values.mipLevel = 0;
		values.width = cubeMapSideLength;
		values.lodBias = _lodBias;

		vkCmdPushConstants(cubeMapCmd, filterPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &values);

		vulkan.beginRenderPass(cubeMapCmd, renderPass, diffuseCubeMapFramebuffer, VkRect2D{ 0u, 0u, cubeMapSideLength, cubeMapSideLength }, clearValues);
		vkCmdDraw(cubeMapCmd, 3, 1u, 0, 0);
		vulkan.endRenderPass(cubeMapCmd);
	}

	////////////////////////////////////////////////////////////////////////////////////////
	//Output

	VkFormat targetFormat = static_cast<VkFormat>(_targetFormat);
	VkImageLayout currentSpecularCubeMapImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkImageLayout currentDiffuseCubeMapImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkImage convertedSpecularCubeMap = VK_NULL_HANDLE;
	VkImage convertedDiffuseCubeMap = VK_NULL_HANDLE;

	if(targetFormat != cubeMapFormat)
	{		
		if ((res = convertVkFormat(vulkan, cubeMapCmd, outputSpecularCubeMap, convertedSpecularCubeMap, targetFormat, currentSpecularCubeMapImageLayout)) != Success)
		{
			printf("Failed to convert Image \n");
			return res;
		}
		currentSpecularCubeMapImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	
		if ((res = convertVkFormat(vulkan, cubeMapCmd, outputDiffuseCubeMap, convertedDiffuseCubeMap, targetFormat, currentDiffuseCubeMapImageLayout)) != Success)
		{
			printf("Failed to convert Image \n");
			return res;
		}
		currentDiffuseCubeMapImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else 
	{
		convertedSpecularCubeMap = outputSpecularCubeMap;
		convertedDiffuseCubeMap = outputDiffuseCubeMap;
	}

	if (vulkan.endCommandBuffer(cubeMapCmd) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}

	if (vulkan.executeCommandBuffer(cubeMapCmd) != VK_SUCCESS)
	{
		return Result::VulkanError;
	}	
	
	KtxImage::Version ktxImageVersion = KtxImage::Version::KTX1;

	switch (_ktxVersion)
	{
	case 1:
		ktxImageVersion = KtxImage::Version::KTX1;
		break;
	case 2:
		ktxImageVersion = KtxImage::Version::KTX2;
		break;
	}

	if (downloadCubemap(vulkan, convertedSpecularCubeMap, _outputPathSpecular, ktxImageVersion, _ktxCompressionQuality, currentSpecularCubeMapImageLayout) != VK_SUCCESS)
	{
		printf("Failed to download Image \n");
		return Result::VulkanError;
	}

	if (downloadCubemap(vulkan, convertedDiffuseCubeMap, _outputPathDiffuse, ktxImageVersion, _ktxCompressionQuality, currentDiffuseCubeMapImageLayout) != VK_SUCCESS)
	{
		printf("Failed to download Image \n");
		return Result::VulkanError;
	}

	return Result::Success;
}
