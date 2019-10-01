#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace IBLLib
{
	class vkHelper
	{
		friend class DescriptorSetInfo;

	public:
		vkHelper();
		~vkHelper();

		VkResult initialize(uint32_t _phyDeviceIndex = 0u, uint32_t _descriptorPoolSizeFactor = 1u, bool _debugOutput = true);

		void shutdown();

		VkResult createCommandBuffer(VkCommandBuffer& _outCmdBuffer, VkCommandBufferLevel _level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

		// command buffers are owned by this vkHelper instance, do not reset or destory manually
		VkResult createCommandBuffers(std::vector<VkCommandBuffer>& _outCmdBuffers, uint32_t _count, VkCommandBufferLevel _level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const;

		void destroyCommandBuffer(VkCommandBuffer _cmdBuffer) const;

		VkResult beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _flags = 0u) const;

		VkResult beginCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers, VkCommandBufferUsageFlags _flags = 0u) const;

		VkResult endCommandBuffer(VkCommandBuffer _cmdBuffers) const;

		VkResult endCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers) const;

		VkResult executeCommandBuffer(VkCommandBuffer _cmdBuffer) const;

		// make sure there are no dependencies between command buffers. this method is blocking
		VkResult executeCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers) const;

		VkResult loadShaderModule(VkShaderModule& _outShader, const uint32_t* _spvBlob, size_t _spvBlobByteSize);

		// shader module is owned by this vkHelper instance
		VkResult loadShaderModule(VkShaderModule& _outShader, const char* _path);

		// TODO: refactor descriptor sets / pipeline layouts into another helper class ?

		// layouts are owned by this vkHelper instance, do not destory manually
		VkResult createDecriptorSetLayout(VkDescriptorSetLayout& _outLayout, const VkDescriptorSetLayoutCreateInfo* _pCreateInfo);

		// this variant adds the created layout to the end of _outLayouts
		VkResult addDecriptorSetLayout(std::vector<VkDescriptorSetLayout>& _outLayouts, const VkDescriptorSetLayoutCreateInfo* _pCreateInfo);

		// sets are owned by this vkHelper instance descriptor pool, dont free manually
		VkResult createDescriptorSet(VkDescriptorSet& _outDescriptorSet, VkDescriptorSetLayout _layout) const;

		// sets are owned by this vkHelper instance descriptor pool, dont free manually
		VkResult createDescriptorSets(std::vector<VkDescriptorSet>& _outDescriptorSets, const std::vector<VkDescriptorSetLayout>& _layouts) const;

		void bindDescriptorSets(VkCommandBuffer _cmdBuffer, VkPipelineLayout _layout, const std::vector<VkDescriptorSet>& _descriptorSets, VkPipelineBindPoint _bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, uint32_t _firstSet = 0u, const std::vector<uint32_t>& _dynamicOffsets = {}) const;
		void bindDescriptorSet(VkCommandBuffer _cmdBuffer, VkPipelineLayout _layout, const VkDescriptorSet _descriptorSets, VkPipelineBindPoint _bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, uint32_t _firstSet = 0u, const std::vector<uint32_t> & _dynamicOffsets = {}) const;

		void updateDescriptorSets(const std::vector<VkWriteDescriptorSet>& _writes = {}, const std::vector<VkCopyDescriptorSet>& _copies = {}) const;

		VkResult createPipelineLayout(VkPipelineLayout& _outLayout, const VkDescriptorSetLayout _descriptorLayouts, const std::vector<VkPushConstantRange>& _pushConstantRanges = {});
		
		// layouts are owned by this vkHelper instance, do not destory manually
		VkResult createPipelineLayout(VkPipelineLayout& _outLayout, const std::vector<VkDescriptorSetLayout>& _descriptorLayouts, const std::vector<VkPushConstantRange>& _pushConstantRanges = {});

		// pipelines are owned by this vkHelper instance, do not destory manually
		VkResult createPipeline(VkPipeline& _outPipeline, const VkGraphicsPipelineCreateInfo* _pCreateInfo);

		// renderpasses are owned by this vkHelper instance, do not destory manually
		VkResult createRenderPass(VkRenderPass& _outRenderPass, const VkRenderPassCreateInfo* _pCreateInfo);

		// returns true if memory type is supported by the device
		bool getMemoryTypeIndex(const VkMemoryRequirements& _requirements, VkMemoryPropertyFlags _properties, uint32_t& _outIndex);

		VkResult createBufferAndAllocate(VkBuffer& _outBuffer, uint32_t _byteSize, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VkSharingMode _sharingMode = VK_SHARING_MODE_EXCLUSIVE, VkBufferCreateFlags _flags = 0u);

		void destroyBuffer(VkBuffer _buffer);

		VkResult writeBufferData(VkBuffer _buffer, const void* _pData, size_t _bytes);
		VkResult readBufferData(VkBuffer _buffer, void* _pData, size_t _bytes, size_t _offset=0u);

		VkResult createImage2DAndAllocate(VkImage& _outImage, uint32_t _width, uint32_t _height,
			VkFormat _format, VkImageUsageFlags _usage,
			uint32_t _mipLevels = 1u, uint32_t _arrayLayers = 1u,
			VkImageTiling _tiling = VK_IMAGE_TILING_OPTIMAL,
			VkMemoryPropertyFlags _memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			VkSharingMode _sharingMode = VK_SHARING_MODE_EXCLUSIVE, 
			VkImageCreateFlags _flags = 0);

		void destroyImage(VkImage _image);

		VkResult createImageView(VkImageView& _outView, VkImage _image, VkImageSubresourceRange _range = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u }, VkFormat _format = VK_FORMAT_UNDEFINED, VkImageViewType _type = VK_IMAGE_VIEW_TYPE_2D, VkComponentMapping  _swizzle = { VK_COMPONENT_SWIZZLE_IDENTITY , VK_COMPONENT_SWIZZLE_IDENTITY ,VK_COMPONENT_SWIZZLE_IDENTITY ,VK_COMPONENT_SWIZZLE_IDENTITY });

		void copyBufferToBasicImage2D(VkCommandBuffer _cmdBuffer, VkBuffer _src, VkImage _dst) const;
		void copyImage2DToBuffer(VkCommandBuffer _cmdBuffer, VkImage _src, VkBuffer _dst, VkImageSubresourceLayers _imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT ,0u, 0u, 1u}) const;
		void copyImage2DToBuffer(VkCommandBuffer _cmdBuffer, VkImage _src, VkBuffer _dst, const VkBufferImageCopy& _region) const;

		void imageBarrier(VkCommandBuffer _cmdBuffer, VkImage _image,
			VkImageLayout _oldLayout, VkImageLayout _newLayout,
			VkPipelineStageFlags _srcStage, VkAccessFlags _srcAccess,
			VkPipelineStageFlags _dstStage, VkAccessFlags _dstAccess,
			VkImageSubresourceRange _subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}) const;

		void transitionImageToTransferWrite(VkCommandBuffer _cmdBuffer, VkImage _image, VkImageLayout _oldLayout = VK_IMAGE_LAYOUT_UNDEFINED) const
		{
			// TODO: lookup old layout from m_images info and write new layout back to info
			imageBarrier(_cmdBuffer, _image, _oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0u, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
		}

		void transitionImageToShaderRead(VkCommandBuffer _cmdBuffer, VkImage _image, VkImageLayout _oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) const
		{
			imageBarrier(_cmdBuffer, _image, _oldLayout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
		}

		void transitionImageToTransferRead(VkCommandBuffer _cmdBuffer, VkImage _image, VkImageLayout _oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) const
		{
			imageBarrier(_cmdBuffer, _image,
				_oldLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // src stage, access
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);//dst stage, access
		}

		// framebuffer are owned by this vkHelper instance, do not destory manually
		VkResult createFramebuffer(VkFramebuffer& _outFramebuffer, VkRenderPass _renderPass, uint32_t _width, uint32_t _height, const std::vector<VkImageView>& _attachments, uint32_t _layers = 1u);

		// simpler helper function using views attached to VkImage
		VkResult createFramebuffer(VkFramebuffer& _outFramebuffer, VkRenderPass _renderPass, VkImage _image);

		void beginRenderPass(VkCommandBuffer _cmdBuffer, VkRenderPass _renderPass, VkFramebuffer _framebuffer, const VkRect2D& _area, const std::vector<VkClearValue>& _clearValues = {}, VkSubpassContents _contents = VK_SUBPASS_CONTENTS_INLINE) const;

		void endRenderPass(VkCommandBuffer _cmdBuffer) const { vkCmdEndRenderPass(_cmdBuffer); };

		void fillSamplerCreateInfo(VkSamplerCreateInfo& _samplerInfo);
		VkResult createSampler(VkSampler& _outSampler, VkSamplerCreateInfo _info);

		const VkImageCreateInfo* getCreateInfo(const VkImage _image);

	private:
		struct Buffer
		{
			VkBufferCreateInfo info{};
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			void destroy(VkDevice _device);
		};

		struct Image
		{
			VkImageCreateInfo info{};
			VkImage image = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			std::vector<VkImageView> views;
			void destroy(VkDevice _device);
		};

		VkInstance m_instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceFeatures m_deviceFeatures{};
		VkPhysicalDeviceMemoryProperties m_memoryProperties{};

		VkDevice m_logicalDevice = VK_NULL_HANDLE;
		VkQueue m_queue = VK_NULL_HANDLE;
		uint32_t m_queueFamilyIndex = 0u;
		VkCommandPool m_commandPool = VK_NULL_HANDLE;
		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
		VkPipelineCache m_pipelineCache = VK_NULL_HANDLE;

		std::vector<VkShaderModule> m_shaderModules;
		std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
		std::vector<VkPipelineLayout> m_pipelineLayouts;
		std::vector<VkPipeline> m_pipelines;
		std::vector<VkRenderPass> m_renderPasses;
		std::vector<VkFramebuffer> m_frameBuffers;
		std::vector<Buffer> m_buffers;
		std::vector<Image> m_images;
		std::vector<VkSampler> m_samplers;

		bool m_debugOutputEnabled;
	};

	class SpecConstantFactory
	{
	public:
		template <class T>
		void addConstant(const T& _data, const uint32_t _constantId = UINT32_MAX)
		{
			m_entries.emplace_back();
			VkSpecializationMapEntry& entry = m_entries.back();

			entry.constantID = _constantId == UINT32_MAX ? m_constantId++ : _constantId;
			entry.offset = static_cast<uint32_t>(m_data.size());
			entry.size = sizeof(T);

			// reserve space
			m_data.resize(entry.offset + entry.size);

			memcpy(m_data.data() + entry.offset, &_data, entry.size);
		}

		const VkSpecializationInfo* getInfo();

	private:
		uint32_t m_constantId = 0u;
		VkSpecializationInfo m_info;

		std::vector<VkSpecializationMapEntry> m_entries;
		std::vector<uint8_t> m_data;
	};

	class DescriptorSetInfo
	{
	public:

		void addCombinedImageSampler(VkSampler _sampler, VkImageView _imageView, VkImageLayout _imageLayout, uint32_t _binding = UINT32_MAX, VkShaderStageFlags _stages = VK_SHADER_STAGE_FRAGMENT_BIT);
		void addUniform(VkBuffer _uniform, VkDeviceSize _offset = 0u, VkDeviceSize _range = VK_WHOLE_SIZE, uint32_t _binding = UINT32_MAX, VkShaderStageFlags _stages = VK_SHADER_STAGE_ALL_GRAPHICS);

		// helper function that creates layout and descriptor set and VkWriteDescriptorSets
		VkResult create(vkHelper& _instance, std::vector<VkDescriptorSetLayout>& _outLayouts, std::vector<VkDescriptorSet>& _outDescriptorSets);
		VkResult create(vkHelper& _instance, VkDescriptorSetLayout& _outLayout, VkDescriptorSet& _outDescriptorSet);

		const VkDescriptorSetLayoutCreateInfo* getLayoutCreateInfo();
		const std::vector<VkWriteDescriptorSet>& getWrites() const { return m_writes; }

	private:
		void addBinding(VkDescriptorType _type, uint32_t _count = 1u, VkShaderStageFlags _stages = VK_SHADER_STAGE_ALL_GRAPHICS, uint32_t _binding = UINT32_MAX, const VkSampler * _immutableSampler = nullptr);
		
	private:
		struct Resource
		{
			Resource(VkBuffer _buffer, VkDeviceSize _offset = 0u, VkDeviceSize _range = VK_WHOLE_SIZE) :
				buffer({ _buffer, _offset, _range }) {}

			Resource(VkSampler _sampler, VkImageView _imageView, VkImageLayout _imageLayout) :
				image({ _sampler, _imageView, _imageLayout }) {}

			union {
				VkDescriptorBufferInfo buffer;
				VkDescriptorImageInfo image;
			};
		};

		std::vector<VkDescriptorSetLayoutBinding> m_bindings;
		std::vector<Resource> m_resources;
		std::vector<VkWriteDescriptorSet> m_writes;

		VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
		VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

		VkDescriptorSetLayoutCreateInfo m_layoutCreateInfo{};
	};

	class GraphicsPipelineDesc
	{
	public:

		GraphicsPipelineDesc();

		// TODO: copy semantics

		void addShaderStage(VkShaderModule _shaderModule, VkShaderStageFlagBits _stage, const char* _entryPoint, const VkSpecializationInfo* _specInfo = nullptr);

		//offset is a byte offset of this attribute relative to the start of an element in the vertex input binding.
		void addVertexAttribute(VkFormat _format, uint32_t _binding, uint32_t _offset, uint32_t _location = UINT32_MAX);
		void addVertexBinding(uint32_t _binding, uint32_t _stride, VkVertexInputRate _rate = VK_VERTEX_INPUT_RATE_VERTEX);

		void addColorBlendAttachment(VkPipelineColorBlendAttachmentState& _attachment);

		void setPrimitiveTopology(VkPrimitiveTopology _topology, bool _enablePrimitiveRestart = false);
		
		void setRenderPass(VkRenderPass _renderPass);
		void setPipelineLayout(VkPipelineLayout _pipelineLayout);
		void setViewportExtent(VkExtent2D _extent);

		const VkGraphicsPipelineCreateInfo* getInfo();
	private:
		VkGraphicsPipelineCreateInfo m_info{};

		std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

		VkPipelineVertexInputStateCreateInfo m_vertexInput{};
		std::vector<VkVertexInputAttributeDescription> m_vertexAttributes;
		std::vector<VkVertexInputBindingDescription> m_vertexBindings;

		std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendAttachments;

		VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
		VkPipelineTessellationStateCreateInfo m_tesselationState{};
		VkViewport m_viewport{};
		VkRect2D m_viewportScissor {};
		VkPipelineViewportStateCreateInfo m_viewportState{};
		VkPipelineRasterizationStateCreateInfo m_rasterState{};
		VkPipelineMultisampleStateCreateInfo m_multiSample{};
		VkPipelineDepthStencilStateCreateInfo m_depthStencilState{};
		VkPipelineColorBlendStateCreateInfo  m_colorBlendState{};
		VkPipelineDynamicStateCreateInfo m_dynamicState{};
	};

	class RenderPassDesc
	{
	public:
		RenderPassDesc();

		// TODO: copy semantics

		void addAttachment(
			VkFormat _format = VK_FORMAT_R8G8B8A8_SRGB,
			VkAttachmentLoadOp _loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VkAttachmentStoreOp _storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT,
			VkImageLayout _initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			VkImageLayout _finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VkAttachmentLoadOp _stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VkAttachmentStoreOp _stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE);

		// TODO: suppasses & dependencies

		const VkRenderPassCreateInfo* getInfo();
	private:
		VkRenderPassCreateInfo m_info{};
		VkSubpassDescription m_subpass{};
		//VkSubpassDependency m_subpassDependency{};
		std::vector<VkAttachmentReference> m_attachmentRefs;
		std::vector<VkAttachmentDescription> m_attachments;
	};
} // IBLLib