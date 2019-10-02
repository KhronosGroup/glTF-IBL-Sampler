#include "vkHelper.h"
#include "FileHelper.h"

constexpr auto g_PipelineCachePath = "pipeline.cache";

IBLLib::vkHelper::vkHelper()
{
}

IBLLib::vkHelper::~vkHelper()
{
	shutdown();
}

VkResult IBLLib::vkHelper::initialize(uint32_t _phyDeviceIndex, uint32_t _descriptorPoolSizeFactor, bool _debugOutput)
{
	VkResult res = VK_RESULT_MAX_ENUM;
	m_debugOutputEnabled = _debugOutput;
	//
	// Create instance
	//

	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "glTF-IBL-Sampler";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "IBLLib";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		std::vector<const char*> layers;
		if (_debugOutput)
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
		}

		uint32_t layerCount = 0u;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (auto it = layers.begin(); it != layers.end();)
		{
			bool supported = false;
			for (const VkLayerProperties& prop : availableLayers)
			{
				if (strcmp(prop.layerName, *it) == 0)
				{	
					printf("Found support for %s layer\n", *it);
					supported = true;
					break;
				}
			}

			if (supported == false)
			{
				it = layers.erase(it);
			}
			else
			{
				++it;
			}
		}

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		if (_debugOutput)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
			createInfo.ppEnabledLayerNames = layers.data();
		}
		// TODO: extensions and layers if needed

		if ((res = vkCreateInstance(&createInfo, nullptr, &m_instance)) != VK_SUCCESS)
		{
			printf("Failed to create Vulkan instance [%u]\n", res);
			return res;
		}

		printf("Vulkan instance created\n");
	}

	//
	// Select physical device
	//

	{
		uint32_t deviceCount = 0;
		if ((res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr)) != VK_SUCCESS)
		{
			printf("Failed to enumerate physical devices [%u]\n", res);
			return res;
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		if ((res = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data())) != VK_SUCCESS)
		{
			printf("Failed to enumerate physical devices [%u]\n", res);
			return res;
		}

		if (_phyDeviceIndex >= deviceCount)
		{
			printf("Invalid physical device index %u, found %u devices\n", _phyDeviceIndex, deviceCount);
			return VK_ERROR_INITIALIZATION_FAILED;
		}

		m_physicalDevice = devices[_phyDeviceIndex];

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);

		printf("Physical Device created: %s\n", deviceProperties.deviceName);
		printf("APIVersion: %u.%u.%u\n", VK_VERSION_MAJOR(deviceProperties.apiVersion), VK_VERSION_MINOR(deviceProperties.apiVersion), VK_VERSION_PATCH(deviceProperties.apiVersion));
		printf("DriverVersion: %u\n", deviceProperties.driverVersion);

		vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures); // TODO: check needed features
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);		
	}

	//
	// Select queue & logical device
	//

	m_queueFamilyIndex = UINT32_MAX;

	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

		for (uint32_t i = 0; i < queueFamilyCount && m_queueFamilyIndex == UINT32_MAX; ++i)
		{
			const VkQueueFamilyProperties& family = queueFamilies[i];

			if (family.queueCount > 0u 
				&& (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				&& (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
				)
			{
				m_queueFamilyIndex = i;
			}
		}

		if (m_queueFamilyIndex == UINT32_MAX)
		{
			printf("Failed to find matching queue family\n");
			return VK_RESULT_MAX_ENUM;
		}

		if (m_debugOutputEnabled)
		{
			printf("Selected queue index %u\n", m_queueFamilyIndex);
		}

		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
		queueCreateInfo.queueCount = 1u;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures{}; // TODO: fill required device features

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
		deviceCreateInfo.queueCreateInfoCount = 1u;
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = 0u;

		if ((res = vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_logicalDevice)) != VK_SUCCESS)
		{
			printf("Failed to create logical device [%u]\n", res);
			return res;
		}

		if (m_debugOutputEnabled)
		{
			printf("Logical device created\n");
		}
		vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 0, &m_queue);
	}

	//
	// Create command pool
	//

	{
		VkCommandPoolCreateInfo cmdPoolCreateInfo{};
		cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolCreateInfo.pNext = nullptr;
		cmdPoolCreateInfo.flags = /*VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | */VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		cmdPoolCreateInfo.queueFamilyIndex = m_queueFamilyIndex;

		if ((res = vkCreateCommandPool(m_logicalDevice, &cmdPoolCreateInfo, nullptr, &m_commandPool)) != VK_SUCCESS)
		{
			printf("Failed to create command pool [%u]\n", res);
			return res;
		}
		if (m_debugOutputEnabled)
		{
			printf("Command pool created\n");
		}
	}

	//
	// Create descriptor pool
	//

	{
		constexpr uint32_t descCount = 8u;
		constexpr uint32_t setCount = 8u;

		// TODO: remove unneeded descriptor types
		VkDescriptorPoolSize sizes[] = {
			{VK_DESCRIPTOR_TYPE_SAMPLER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, descCount * _descriptorPoolSizeFactor},
			{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, descCount * _descriptorPoolSizeFactor}
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.pNext = nullptr;
		descriptorPoolCreateInfo.pPoolSizes = sizes;
		descriptorPoolCreateInfo.poolSizeCount = sizeof(sizes) / sizeof(VkDescriptorPoolSize);
		descriptorPoolCreateInfo.maxSets = setCount * _descriptorPoolSizeFactor;

		if ((res = vkCreateDescriptorPool(m_logicalDevice, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool)) != VK_SUCCESS)
		{
			printf("Failed to create descriptor pool [%u]\n", res);
			return res;
		}

		if (m_debugOutputEnabled)
		{
			printf("Descriptor pool created\n");
		}
	}

	//
	// Create pipeline cache
	//

	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo{};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		std::vector<char> cache;
		if (readFile(g_PipelineCachePath, cache))
		{
			printf("Vulkan pipeline cache loaded\n");
			
			pipelineCacheCreateInfo.initialDataSize = static_cast<uint32_t>(cache.size());
			pipelineCacheCreateInfo.pInitialData = cache.data();
		}

		if ((res = vkCreatePipelineCache(m_logicalDevice, &pipelineCacheCreateInfo, nullptr, &m_pipelineCache)) != VK_SUCCESS)
		{
			printf("Failed to create pipeline cache [%u]\n", res);
			return res;
		}
	}

	return res;
}

void IBLLib::vkHelper::shutdown()
{
	if (m_logicalDevice != VK_NULL_HANDLE)
	{
		// clear framebuffer
		for (const VkFramebuffer& framebuf : m_frameBuffers)
		{
			vkDestroyFramebuffer(m_logicalDevice, framebuf, nullptr);
		}
		m_frameBuffers.clear();

		for (const VkSampler& sampler : m_samplers)
		{
			vkDestroySampler(m_logicalDevice, sampler, nullptr);
		}
		m_samplers.clear();

		// clear images
		for (Image& img : m_images)
		{
			img.destroy(m_logicalDevice);
		}
		m_images.clear();

		// clear buffers
		for (Buffer& buf : m_buffers)
		{
			buf.destroy(m_logicalDevice);
		}
		m_buffers.clear();

		// clear pipelines
		for (const VkPipeline& pipeline : m_pipelines)
		{
			vkDestroyPipeline(m_logicalDevice, pipeline, nullptr);
		}
		m_pipelines.clear();

		// clear pipeline layouts
		for (const VkPipelineLayout& layout : m_pipelineLayouts)
		{
			vkDestroyPipelineLayout(m_logicalDevice, layout, nullptr);
		}
		m_pipelineLayouts.clear();

		// clear set layouts
		for (const VkDescriptorSetLayout& layout : m_descriptorSetLayouts)
		{
			vkDestroyDescriptorSetLayout(m_logicalDevice, layout, nullptr);
		}
		m_descriptorSetLayouts.clear();

		if (m_descriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(m_logicalDevice, m_descriptorPool, nullptr);
			if (m_debugOutputEnabled)
			{
				printf("Vulkan descriptor pool destroyed\n");
			}
			m_descriptorPool = VK_NULL_HANDLE;
		}

		if (m_pipelineCache != VK_NULL_HANDLE)
		{
			// store the pipeline cache
			size_t bytes = 0u;
			if (vkGetPipelineCacheData(m_logicalDevice, m_pipelineCache, &bytes, nullptr) == VK_SUCCESS)
			{
				std::vector<char> cache;
				cache.resize(bytes);

				if (vkGetPipelineCacheData(m_logicalDevice, m_pipelineCache, &bytes, cache.data()) == VK_SUCCESS)
				{
					if (writeFile(g_PipelineCachePath, cache))
					{
						printf("Stored %s [%zukb]\n", g_PipelineCachePath, cache.size() / 1000u);
					}
				}				
			}

			vkDestroyPipelineCache(m_logicalDevice, m_pipelineCache, nullptr);
			if (m_debugOutputEnabled)
			{
				printf("Vulkan pipeline cache destroyed\n");
			}
			m_pipelineCache = VK_NULL_HANDLE;
		}

		for (const VkRenderPass& renderpass : m_renderPasses)
		{
			vkDestroyRenderPass(m_logicalDevice, renderpass, nullptr);
		}
		m_renderPasses.clear();

		for (const VkShaderModule& shader : m_shaderModules)
		{
			vkDestroyShaderModule(m_logicalDevice, shader, nullptr);
		}
		m_shaderModules.clear();

		if (m_commandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
			if (m_debugOutputEnabled)
			{
				printf("Vulkan command pool destroyed\n");
			}
			m_commandPool = VK_NULL_HANDLE;
		}

		vkDestroyDevice(m_logicalDevice, nullptr);
		if (m_debugOutputEnabled)
		{
			printf("Vulkan logical device destroyed\n");
		}
		m_logicalDevice = VK_NULL_HANDLE;
	}

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, nullptr);
		if (m_debugOutputEnabled)
		{
			printf("Vulkan instance destroyed\n");
		}
		m_instance = VK_NULL_HANDLE;
	}
}

VkResult IBLLib::vkHelper::createCommandBuffer(VkCommandBuffer& _outCmdBuffer, VkCommandBufferLevel _level) const
{
	if (m_commandPool == VK_NULL_HANDLE || m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = _level;
	allocInfo.commandBufferCount = 1u;

	VkResult res = vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, &_outCmdBuffer);

	if (res != VK_SUCCESS)
	{
		printf("Failed to allocate command buffers [%u]\n", res);
	}

	return res;
}

VkResult IBLLib::vkHelper::createCommandBuffers(std::vector<VkCommandBuffer>& _outCmdBuffers, uint32_t _count, VkCommandBufferLevel _level) const
{
	if (m_commandPool == VK_NULL_HANDLE || m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	_outCmdBuffers.resize(_count, VK_NULL_HANDLE);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = _level;
	allocInfo.commandBufferCount = _count;

	VkResult res = vkAllocateCommandBuffers(m_logicalDevice, &allocInfo, _outCmdBuffers.data());

	if (res != VK_SUCCESS)
	{
		printf("Failed to allocate command buffers [%u]\n", res);
	}

	return res;
}

void IBLLib::vkHelper::destroyCommandBuffer(VkCommandBuffer _cmdBuffer) const
{
	if (m_commandPool != VK_NULL_HANDLE && m_logicalDevice != VK_NULL_HANDLE)
	{
		vkFreeCommandBuffers(m_logicalDevice, m_commandPool, 1u, &_cmdBuffer);
	}
}

VkResult IBLLib::vkHelper::beginCommandBuffer(VkCommandBuffer _cmdBuffer, VkCommandBufferUsageFlags _flags) const
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = _flags;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult res = VK_SUCCESS;

	if ((res = vkBeginCommandBuffer(_cmdBuffer, &beginInfo)) != VK_SUCCESS)
	{
		printf("Failed to start recording command buffers [%u]\n", res);
	}
	
	return res;
}

VkResult IBLLib::vkHelper::beginCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers, VkCommandBufferUsageFlags _flags) const
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = _flags;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult res = VK_SUCCESS;

	for (const VkCommandBuffer& buffer : _cmdBuffers)
	{
		if ((res = vkBeginCommandBuffer(buffer, &beginInfo)) != VK_SUCCESS)
		{
			printf("Failed to start recording command buffers [%u]\n", res);
			return res;
		}
	}

	return res;
}

VkResult IBLLib::vkHelper::endCommandBuffer(VkCommandBuffer _cmdBuffers) const
{
	VkResult res = VK_SUCCESS;

	if ((res = vkEndCommandBuffer(_cmdBuffers)) != VK_SUCCESS)
	{
		printf("Failed to end recording command buffers [%u]\n", res);
	}

	return res;
}

VkResult IBLLib::vkHelper::endCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers) const
{
	VkResult res = VK_SUCCESS;

	for (const VkCommandBuffer& buffer : _cmdBuffers)
	{
		if ((res = vkEndCommandBuffer(buffer)) != VK_SUCCESS)
		{
			printf("Failed to end recording command buffers [%u]\n", res);
			return res;
		}
	}

	return res;
}

VkResult IBLLib::vkHelper::executeCommandBuffer(VkCommandBuffer _cmdBuffer) const
{
	return executeCommandBuffers({ _cmdBuffer });
}

VkResult IBLLib::vkHelper::executeCommandBuffers(const std::vector<VkCommandBuffer>& _cmdBuffers) const
{
	if (m_queue == VK_NULL_HANDLE || m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;
	VkFence fence = VK_NULL_HANDLE;

	// create fence
	{
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = nullptr;
		fenceInfo.flags = 0u;

		if ((res = vkCreateFence(m_logicalDevice, &fenceInfo, nullptr, &fence)) != VK_SUCCESS)
		{
			printf("Failed to end create fence [%u]\n", res);
			return res;
		}
	}

	// submit
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pNext = nullptr;
		submitInfo.commandBufferCount = static_cast<uint32_t>(_cmdBuffers.size());
		submitInfo.pCommandBuffers = _cmdBuffers.data();

		if ((res = vkQueueSubmit(m_queue, 1u, &submitInfo, fence)) != VK_SUCCESS)
		{
			printf("Failed to submit queue [%u]\n", res);
			vkDestroyFence(m_logicalDevice, fence, nullptr);
			return res;
		}
		if (m_debugOutputEnabled)
		{
			printf("Executing %u command buffers\n", submitInfo.commandBufferCount);
		}
	}

	// wait / block for execution to be complete
	if ((res = vkWaitForFences(m_logicalDevice, 1u, &fence, VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
	{
		printf("Failed to wait for fence [%u]\n", res);
	}

	if ((res = vkQueueWaitIdle(m_queue)) != VK_SUCCESS)
	{
		printf("Failed to wait for queue [%u]\n", res);
	}

	vkDestroyFence(m_logicalDevice, fence, nullptr);

	return res;
}

VkResult IBLLib::vkHelper::loadShaderModule(VkShaderModule& _outShader, const uint32_t* _spvBlob, size_t _spvBlobByteSize)
{
	if (_spvBlobByteSize % sizeof(uint32_t) != 0u)
	{
		printf("Invalid SPIR-V blob size\n");
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_RESULT_MAX_ENUM;

	// create shader module from spv blob
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = _spvBlobByteSize;
	createInfo.pCode = _spvBlob;

	if ((res = vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &_outShader)) != VK_SUCCESS)
	{
		_outShader = VK_NULL_HANDLE;
		printf("Failed to create shader module [%u]\n", res);
		return res;
	}

	m_shaderModules.emplace_back(_outShader);

	return res;
}

VkResult IBLLib::vkHelper::loadShaderModule(VkShaderModule& _outShader, const char* _path)
{
	std::vector<char> blob;

	if (readFile(_path, blob) == false)
	{
		return VK_RESULT_MAX_ENUM;
	}

	return loadShaderModule(_outShader, reinterpret_cast<const uint32_t*>(blob.data()), static_cast<uint32_t>(blob.size()));
}

VkResult IBLLib::vkHelper::createDecriptorSetLayout(VkDescriptorSetLayout& _outLayout, const VkDescriptorSetLayoutCreateInfo* _pCreateInfo)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	if ((res = vkCreateDescriptorSetLayout(m_logicalDevice, _pCreateInfo, nullptr, &_outLayout)) != VK_SUCCESS)
	{
		_outLayout = VK_NULL_HANDLE;
		printf("Failed to create descriptor set layout [%u]\n", res);
		return res;
	}

	m_descriptorSetLayouts.emplace_back(_outLayout);

	return res;
}

VkResult IBLLib::vkHelper::addDecriptorSetLayout(std::vector<VkDescriptorSetLayout>& _outLayouts, const VkDescriptorSetLayoutCreateInfo* _pCreateInfo)
{
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkResult res = createDecriptorSetLayout(setLayout, _pCreateInfo);

	if (res == VK_SUCCESS)
	{
		_outLayouts.emplace_back(setLayout);
	}

	return res;
}

VkResult IBLLib::vkHelper::createDescriptorSet(VkDescriptorSet& _outDescriptorSet, VkDescriptorSetLayout _layout) const
{
	if (m_logicalDevice == VK_NULL_HANDLE || m_descriptorPool == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	VkDescriptorSetAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.pSetLayouts = &_layout;
	info.descriptorSetCount = 1u;
	info.descriptorPool = m_descriptorPool;

	if ((res = vkAllocateDescriptorSets(m_logicalDevice, &info, &_outDescriptorSet)) != VK_SUCCESS)
	{
		printf("Failed to allocate descriptor set [%u]\n", res);
	}

	return res;
}

VkResult IBLLib::vkHelper::createDescriptorSets(std::vector<VkDescriptorSet>& _outDescriptorSets, const std::vector<VkDescriptorSetLayout>& _layouts) const
{
	if (m_logicalDevice == VK_NULL_HANDLE || m_descriptorPool == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	_outDescriptorSets.resize(_layouts.size());

	VkDescriptorSetAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.pSetLayouts = _layouts.data();
	info.descriptorSetCount = static_cast<uint32_t>(_layouts.size());
	info.descriptorPool = m_descriptorPool;

	if ((res = vkAllocateDescriptorSets(m_logicalDevice, &info, _outDescriptorSets.data())) != VK_SUCCESS)
	{
		printf("Failed to allocate descriptor sets [%u]\n", res);
	}

	return res;
}

void IBLLib::vkHelper::bindDescriptorSets(VkCommandBuffer _cmdBuffer, VkPipelineLayout _layout, const std::vector<VkDescriptorSet>& _descriptorSets, VkPipelineBindPoint _bindPoint, uint32_t _firstSet, const std::vector<uint32_t>& _dynamicOffsets) const
{
	vkCmdBindDescriptorSets(_cmdBuffer, _bindPoint, _layout, _firstSet, static_cast<uint32_t>(_descriptorSets.size()), _descriptorSets.data(),  static_cast<uint32_t>(_dynamicOffsets.size()), _dynamicOffsets.data());
}

void IBLLib::vkHelper::bindDescriptorSet(VkCommandBuffer _cmdBuffer, VkPipelineLayout _layout, const VkDescriptorSet _descriptorSets, VkPipelineBindPoint _bindPoint, uint32_t _firstSet, const std::vector<uint32_t>& _dynamicOffsets) const
{
	vkCmdBindDescriptorSets(_cmdBuffer, _bindPoint, _layout, _firstSet, 1u, &_descriptorSets, static_cast<uint32_t>(_dynamicOffsets.size()), _dynamicOffsets.data());
}

VkResult IBLLib::vkHelper::createPipelineLayout(VkPipelineLayout& _outLayout, const std::vector<VkDescriptorSetLayout>& _descriptorLayouts, const std::vector<VkPushConstantRange>& _pushConstantRanges)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0u;
	info.pSetLayouts = _descriptorLayouts.data();
	info.setLayoutCount = static_cast<uint32_t>(_descriptorLayouts.size());
	info.pPushConstantRanges = _pushConstantRanges.data();
	info.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size());

	if ((res = vkCreatePipelineLayout(m_logicalDevice, &info, nullptr, &_outLayout)) != VK_SUCCESS)
	{
		_outLayout = VK_NULL_HANDLE;
		printf("Failed to create pipeline layout [%u]\n", res);
		return res;
	}

	m_pipelineLayouts.emplace_back(_outLayout);

	return res;
}


VkResult IBLLib::vkHelper::createPipelineLayout(VkPipelineLayout& _outLayout, const VkDescriptorSetLayout _descriptorLayouts, const std::vector<VkPushConstantRange>& _pushConstantRanges)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = 0u;
	info.pSetLayouts = &_descriptorLayouts;
	info.setLayoutCount = 1u;
	info.pPushConstantRanges = _pushConstantRanges.data();
	info.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size());

	if ((res = vkCreatePipelineLayout(m_logicalDevice, &info, nullptr, &_outLayout)) != VK_SUCCESS)
	{
		_outLayout = VK_NULL_HANDLE;
		printf("Failed to create pipeline layout [%u]\n", res);
		return res;
	}

	m_pipelineLayouts.emplace_back(_outLayout);

	return res;
}

VkResult IBLLib::vkHelper::createPipeline(VkPipeline& _outPipeline, const VkGraphicsPipelineCreateInfo* _pCreateInfo)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	if ((res = vkCreateGraphicsPipelines(m_logicalDevice, m_pipelineCache, 1u, _pCreateInfo, nullptr, &_outPipeline)) != VK_SUCCESS)
	{
		_outPipeline = VK_NULL_HANDLE;
		printf("Failed to create pipeline [%u]\n", res);
		return res;
	}

	m_pipelines.emplace_back(_outPipeline);

	return res;
}

VkResult IBLLib::vkHelper::createRenderPass(VkRenderPass& _outRenderPass, const VkRenderPassCreateInfo* _pCreateInfo)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	if ((res = vkCreateRenderPass(m_logicalDevice, _pCreateInfo, nullptr, &_outRenderPass)) != VK_SUCCESS)
	{
		_outRenderPass = VK_NULL_HANDLE;
		printf("Failed to create renderapass [%u]\n", res);
		return res;
	}

	m_renderPasses.emplace_back(_outRenderPass);

	return res;
}

bool IBLLib::vkHelper::getMemoryTypeIndex(const VkMemoryRequirements& _requirements, VkMemoryPropertyFlags _properties, uint32_t& _outIndex)
{
	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		return false;
	}

	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
	{
		if ((_requirements.memoryTypeBits & (1 << i)) &&
			(m_memoryProperties.memoryTypes[i].propertyFlags & _properties))
		{
			_outIndex = i;
			return true;
		}
	}

	return false;
}

VkResult IBLLib::vkHelper::createBufferAndAllocate(VkBuffer& _outBuffer, uint32_t _byteSize, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _memoryFlags, VkSharingMode _sharingMode, VkBufferCreateFlags _flags)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = _byteSize;
	bufferInfo.usage = _usage;
	bufferInfo.sharingMode = _sharingMode;
	bufferInfo.pQueueFamilyIndices = &m_queueFamilyIndex;
	bufferInfo.queueFamilyIndexCount = 1u;
	bufferInfo.flags = _flags;

	VkResult res = VK_SUCCESS;

	if ((res = vkCreateBuffer(m_logicalDevice, &bufferInfo, nullptr, &_outBuffer)) != VK_SUCCESS)
	{
		_outBuffer = VK_NULL_HANDLE;
		printf("Failed to create buffer [%u]\n", res);
		return res;
	}

	m_buffers.emplace_back();
	Buffer& buffer = m_buffers.back();

	buffer.buffer = _outBuffer;
	buffer.info = bufferInfo;

	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(m_logicalDevice, _outBuffer, &requirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = requirements.size;

	if (getMemoryTypeIndex(requirements, _memoryFlags, allocInfo.memoryTypeIndex) == false)
	{
		printf("Unsupported memory requirements [%u]\n", res);
		return res;
	}

	if ((res = vkAllocateMemory(m_logicalDevice, &allocInfo, nullptr, &buffer.memory)) != VK_SUCCESS)
	{
		printf("Failed to allocate buffer [%u]\n", res);
		return res;
	}

	if ((res = vkBindBufferMemory(m_logicalDevice, _outBuffer, buffer.memory, 0u)) != VK_SUCCESS)
	{
		printf("Failed to bind buffer memory [%u]\n", res);
	}

	return res;
}

void IBLLib::vkHelper::destroyBuffer(VkBuffer _buffer)
{
	if (m_logicalDevice != VK_NULL_HANDLE)
	{
		for (auto it = m_buffers.begin(), end = m_buffers.end(); it != end; ++it)
		{
			if (it->buffer == _buffer)
			{
				it->destroy(m_logicalDevice);
				m_buffers.erase(it);
				break;
			}
		}
	}
}

VkResult IBLLib::vkHelper::writeBufferData(VkBuffer _buffer, const void* _pData, size_t _bytes)
{
	VkResult res = VK_RESULT_MAX_ENUM;

	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return res;
	}

	for (const Buffer& buf : m_buffers)
	{
		if (buf.buffer == _buffer && buf.memory != VK_NULL_HANDLE)
		{
			void* data = nullptr;
			if ((res = vkMapMemory(m_logicalDevice, buf.memory, 0u, _bytes, 0, &data)) != VK_SUCCESS)
			{
				printf("Failed to map buffer memory [%u]\n", res);
				return res;
			}

			// write data
			memcpy(data, _pData, _bytes);
			vkUnmapMemory(m_logicalDevice, buf.memory);
			return res;
		}
	}

	printf("Not a valid buffer\n");

	return res;
}

VkResult IBLLib::vkHelper::readBufferData(VkBuffer _buffer, void* _pData, size_t _bytes, size_t _offset)
{
	VkResult res = VK_RESULT_MAX_ENUM;

	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return res;
	}

	for (const Buffer& buf : m_buffers)
	{
		if (buf.buffer == _buffer && buf.memory != VK_NULL_HANDLE)
		{
			void* data = nullptr;
			if ((res = vkMapMemory(m_logicalDevice, buf.memory, _offset, _bytes, 0, &data)) != VK_SUCCESS)
			{
				printf("Failed to map buffer memory [%u]\n", res);
				return res;
			}

			// read data
			memcpy(_pData, data, _bytes);			

			vkUnmapMemory(m_logicalDevice, buf.memory);
			return res;
		}
	}

	printf("Not a valid buffer\n");

	return res;
}

VkResult IBLLib::vkHelper::createImage2DAndAllocate(
	VkImage& _outImage, uint32_t _width, uint32_t _height,
	VkFormat _format, VkImageUsageFlags _usage, 
	uint32_t _mipLevels, uint32_t _arrayLayers,
	VkImageTiling _tiling, VkMemoryPropertyFlags _memoryFlags, VkSharingMode _sharingMode, VkImageCreateFlags _flags)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = _width;
	imageInfo.extent.height = _height;
	imageInfo.extent.depth = 1u;
	imageInfo.mipLevels = _mipLevels;
	imageInfo.arrayLayers = _arrayLayers;
	imageInfo.format = _format;
	imageInfo.tiling = _tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = _usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = _sharingMode;
	imageInfo.flags = _flags;

	VkResult res = VK_RESULT_MAX_ENUM;

	if ((res = vkCreateImage(m_logicalDevice, &imageInfo, nullptr, &_outImage)) != VK_SUCCESS)
	{
		printf("Failed to create image [%u]\n", res);
		return res;
	}

	m_images.emplace_back();
	Image& img = m_images.back();

	img.image = _outImage;
	img.info = imageInfo;

	VkMemoryRequirements requirements{};
	vkGetImageMemoryRequirements(m_logicalDevice, _outImage, &requirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = requirements.size;

	if (getMemoryTypeIndex(requirements, _memoryFlags, allocInfo.memoryTypeIndex) == false)
	{
		printf("Unsupported memory requirements [%u]\n", res);
		return res;
	}

	if ((res = vkAllocateMemory(m_logicalDevice, &allocInfo, nullptr, &img.memory)) != VK_SUCCESS)
	{
		printf("Failed to allocate image [%u]\n", res);
		return res;
	}

	if ((res = vkBindImageMemory(m_logicalDevice, _outImage, img.memory, 0u)) != VK_SUCCESS)
	{
		printf("Failed to bind image memory [%u]\n", res);
	}

	return res;
}

void IBLLib::vkHelper::Image::destroy(VkDevice _device)
{
	for (const VkImageView& view : views)
	{
		vkDestroyImageView(_device, view, nullptr);
	}
	views.clear();

	if (image != VK_NULL_HANDLE)
	{
		vkDestroyImage(_device, image, nullptr);
		image = VK_NULL_HANDLE;
	}

	if (memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, memory, nullptr);
		memory = VK_NULL_HANDLE;
	}
}

void IBLLib::vkHelper::Buffer::destroy(VkDevice _device)
{
	if (buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(_device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
	}

	if (memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(_device, memory, nullptr);
		memory = VK_NULL_HANDLE;
	}
}

void IBLLib::vkHelper::destroyImage(VkImage _image)
{
	if (m_logicalDevice != VK_NULL_HANDLE)
	{
		for (auto it = m_images.begin(), end = m_images.end(); it != end; ++it)
		{
			if (it->image == _image)
			{
				it->destroy(m_logicalDevice);
				m_images.erase(it);
				break;
			}
		}
	}
}

VkResult IBLLib::vkHelper::createImageView(VkImageView& _outView, VkImage _image, VkImageSubresourceRange _range, VkFormat _format, VkImageViewType _type, VkComponentMapping _swizzle)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	for (Image& img : m_images)
	{
		if (img.image == _image)
		{
			VkImageViewCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.pNext = nullptr;
			info.format = _format == VK_FORMAT_UNDEFINED ? img.info.format : _format;
			info.flags = 0u;
			info.image = _image;
			info.components = _swizzle;
			info.viewType = _type;
			info.subresourceRange = _range;

			VkResult res = vkCreateImageView(m_logicalDevice, &info, nullptr, &_outView);

			if (res == VK_SUCCESS)
			{
				img.views.emplace_back(_outView);			
			}
			else
			{
				printf("Failed to create image view [%u]\n", res);
			}

			return res;
		}
	}

	return VK_RESULT_MAX_ENUM;
}

void IBLLib::vkHelper::copyBufferToBasicImage2D(VkCommandBuffer _cmdBuffer, VkBuffer _src, VkImage _dst) const
{
	for (const Image& img : m_images)
	{
		if (img.image == _dst)
		{
			VkBufferImageCopy region{};
			region.bufferOffset = 0u;
			region.bufferRowLength = 0u;
			region.bufferImageHeight = 0u;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0u;
			region.imageSubresource.baseArrayLayer = 0u;
			region.imageSubresource.layerCount = img.info.arrayLayers;// 1u;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = img.info.extent;

			vkCmdCopyBufferToImage(_cmdBuffer, _src, _dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1u, &region);
			return;
		}
	}
}

void IBLLib::vkHelper::copyImage2DToBuffer(VkCommandBuffer _cmdBuffer, VkImage _src, VkBuffer _dst, VkImageSubresourceLayers _imageSubresource) const
{
	for (const Image& img : m_images)
	{
		if (img.image == _src)
		{
			VkBufferImageCopy region{};
			region.bufferOffset = 0u;
			region.bufferRowLength = 0u;
			region.bufferImageHeight = 0u;

			region.imageSubresource = _imageSubresource;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = img.info.extent;
		
			vkCmdCopyImageToBuffer(_cmdBuffer, _src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				_dst,	//	VkBuffer
				1u,		//	uint32_t  regionCount,
				&region	//	const VkBufferImageCopy* pRegions);
				);

			return;
		}
	}

	printf("image not found\n");
}

void IBLLib::vkHelper::copyImage2DToBuffer(VkCommandBuffer _cmdBuffer, VkImage _src, VkBuffer _dst, const VkBufferImageCopy& _region) const
{
	vkCmdCopyImageToBuffer(_cmdBuffer, _src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		_dst,	//	VkBuffer
		1u,		//	uint32_t  regionCount,
		&_region);
}

void IBLLib::vkHelper::imageBarrier(VkCommandBuffer _cmdBuffer, VkImage _image, 
									VkImageLayout oldLayout, VkImageLayout newLayout, 
									VkPipelineStageFlags _srcStage, VkAccessFlags _srcAccess, 
									VkPipelineStageFlags _dstStage, VkAccessFlags _dstAccess, 
									VkImageSubresourceRange _subresourceRange) const
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = _image;
	barrier.subresourceRange = _subresourceRange;
	barrier.srcAccessMask = _srcAccess;
	barrier.dstAccessMask = _dstAccess;

	vkCmdPipelineBarrier(
		_cmdBuffer,
		_srcStage, _dstStage,
		0u,
		0u, nullptr,
		0u, nullptr,
		1u, &barrier
	);
}

VkResult IBLLib::vkHelper::createFramebuffer(VkFramebuffer& _outFramebuffer, VkRenderPass _renderPass, uint32_t _width, uint32_t _height, const std::vector<VkImageView>& _attachments, uint32_t _layers)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_RESULT_MAX_ENUM;

	VkFramebufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = _renderPass;
	info.attachmentCount = static_cast<uint32_t>(_attachments.size());
	info.pAttachments = _attachments.data();
	info.width = _width;
	info.height = _height;
	info.layers = _layers;
	info.flags = 0u;

	if ((res = vkCreateFramebuffer(m_logicalDevice, &info, nullptr, &_outFramebuffer)) != VK_SUCCESS)
	{
		_outFramebuffer = VK_NULL_HANDLE;
		printf("Failed to create framebuffer [%u]\n", res);
		return res;
	}

	m_frameBuffers.emplace_back(_outFramebuffer);

	return res;
}

VkResult IBLLib::vkHelper::createFramebuffer(VkFramebuffer& _outFramebuffer, VkRenderPass _renderPass, VkImage _image)
{
	for (const Image& img : m_images)
	{
		if (img.image == _image)
		{
			return createFramebuffer(_outFramebuffer, _renderPass, img.info.extent.width, img.info.extent.height, img.views, img.info.arrayLayers);
		}
	}

	return VK_RESULT_MAX_ENUM;
}

void IBLLib::vkHelper::beginRenderPass(VkCommandBuffer _cmdBuffer, VkRenderPass _renderPass, VkFramebuffer _framebuffer, const VkRect2D& _area, const std::vector<VkClearValue>& _clearValues, VkSubpassContents _contents) const
{
	VkRenderPassBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.pNext = nullptr;
	info.renderPass = _renderPass;
	info.framebuffer = _framebuffer;
	info.renderArea = _area;
	info.clearValueCount = static_cast<uint32_t>(_clearValues.size());
	info.pClearValues = _clearValues.data();
	
	vkCmdBeginRenderPass(_cmdBuffer, &info, _contents);
}

void IBLLib::vkHelper::fillSamplerCreateInfo(VkSamplerCreateInfo& _samplerInfo)
{
	_samplerInfo.magFilter = VK_FILTER_LINEAR;
	_samplerInfo.minFilter = VK_FILTER_LINEAR;
	_samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	_samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	_samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT; 
	
	_samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	_samplerInfo.mipLodBias = 0.f;
	_samplerInfo.minLod = 0.f;
	_samplerInfo.maxLod = 1.f;
	
	_samplerInfo.anisotropyEnable = VK_FALSE;
	_samplerInfo.maxAnisotropy = 0.f;
	_samplerInfo.compareEnable = VK_FALSE;
	_samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	
	_samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
}

VkResult IBLLib::vkHelper::createSampler(VkSampler& _outSampler, VkSamplerCreateInfo _info)
{
	if (m_logicalDevice == VK_NULL_HANDLE)
	{
		return VK_RESULT_MAX_ENUM;
	}

	VkResult res = VK_SUCCESS;

	_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	_info.pNext = nullptr;
	_info.flags = 0u;
	_info.unnormalizedCoordinates = VK_FALSE;

	if ((res = vkCreateSampler(m_logicalDevice, &_info, nullptr, &_outSampler)) != VK_SUCCESS)
	{
		printf("Failed to create sampler [%u]\n", res);
	}
	else
	{
		m_samplers.emplace_back(_outSampler);
	}

	return res;
}

const VkImageCreateInfo* IBLLib::vkHelper::getCreateInfo(const VkImage _image)
{
	for (const Image& img : m_images)
	{
		if (img.image == _image)
		{
			return &img.info;
		}
	}

	return nullptr;
}

const VkSpecializationInfo* IBLLib::SpecConstantFactory::getInfo()
{
	m_info.dataSize = static_cast<uint32_t>(m_data.size());
	m_info.pData = m_data.data();

	m_info.mapEntryCount = static_cast<uint32_t>(m_entries.size());
	m_info.pMapEntries = m_entries.data();

	return &m_info;
}

const VkDescriptorSetLayoutCreateInfo* IBLLib::DescriptorSetInfo::getLayoutCreateInfo()
{
	m_layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	m_layoutCreateInfo.pNext = nullptr;
	m_layoutCreateInfo.pBindings = m_bindings.data();
	m_layoutCreateInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
	m_layoutCreateInfo.flags = 0u;

	return &m_layoutCreateInfo;
};

void IBLLib::DescriptorSetInfo::addBinding(VkDescriptorType _type, uint32_t _count, VkShaderStageFlags _stages , uint32_t _binding , const VkSampler* _immutableSampler)
{
	m_bindings.emplace_back();
	VkDescriptorSetLayoutBinding& binding = m_bindings.back();

	binding.binding = _binding == UINT32_MAX ? static_cast<uint32_t>(m_bindings.size() - 1u) : _binding;
	binding.descriptorCount = _count;
	binding.pImmutableSamplers = _immutableSampler;
	binding.stageFlags = _stages;
	binding.descriptorType = _type;
}

void IBLLib::DescriptorSetInfo::addCombinedImageSampler(VkSampler _sampler, VkImageView _imageView, VkImageLayout _imageLayout, uint32_t _binding, VkShaderStageFlags _stages)
{
	addBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1u, _stages, _binding);
	m_resources.emplace_back(_sampler, _imageView, _imageLayout);
}

void IBLLib::DescriptorSetInfo::addUniform(VkBuffer _uniform, VkDeviceSize _offset, VkDeviceSize _range, uint32_t _binding, VkShaderStageFlags _stages)
{
	addBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1u, _stages, _binding);
	m_resources.emplace_back(_uniform, _offset, _range);
}

VkResult IBLLib::DescriptorSetInfo::create(vkHelper& _instance, std::vector<VkDescriptorSetLayout>& _outLayouts, std::vector<VkDescriptorSet>& _outDescriptorSets)
{
	_outLayouts.emplace_back();
	_outDescriptorSets.emplace_back();

	return create(_instance, _outLayouts.back(), _outDescriptorSets.back());
}

VkResult IBLLib::DescriptorSetInfo::create(vkHelper& _instance, VkDescriptorSetLayout& _outLayout, VkDescriptorSet& _outDescriptorSet)
{
	VkResult res = _instance.createDecriptorSetLayout(m_layout, getLayoutCreateInfo());

	if (res != VK_SUCCESS)
	{
		return res;
	}

	_outLayout = m_layout;

	if ((res = _instance.createDescriptorSet(m_descriptorSet, m_layout)) != VK_SUCCESS)
	{
		return res;
	}

	_outDescriptorSet = m_descriptorSet;

	if (m_bindings.size() != m_resources.size())
	{
		return VK_RESULT_MAX_ENUM;
	}

	m_writes.resize(m_resources.size());

	for (size_t i = 0; i < m_resources.size(); i++)
	{
		const VkDescriptorSetLayoutBinding& binding = m_bindings[i];

		VkWriteDescriptorSet& write = m_writes[i];
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.descriptorType = binding.descriptorType;

		// TODO: support arrays
		if (binding.descriptorCount > 1u)
		{
			continue;
		}

		write.descriptorCount = 1u; //binding.descriptorCount;
		write.dstArrayElement = 0u;
		write.dstBinding = binding.binding;
		write.dstSet = m_descriptorSet;

		if (write.descriptorType <= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			write.pImageInfo = &m_resources[i].image;
		}
		else if (write.descriptorType <= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
		{
			write.pBufferInfo = &m_resources[i].buffer;
		}
	}

	return res;
}

void IBLLib::vkHelper::updateDescriptorSets(const std::vector<VkWriteDescriptorSet>& _writes, const std::vector<VkCopyDescriptorSet>& _copies) const
{
	vkUpdateDescriptorSets(m_logicalDevice, static_cast<uint32_t>(_writes.size()), _writes.data(), static_cast<uint32_t>(_copies.size()), _copies.data());
}

IBLLib::GraphicsPipelineDesc::GraphicsPipelineDesc()
{
	// defaults
	m_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	m_info.pNext = nullptr;
	m_info.subpass = 0u;
	m_info.basePipelineHandle = VK_NULL_HANDLE;
	m_info.basePipelineIndex = 0u;
	m_info.flags = 0u;

	m_vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_vertexInput.pNext = nullptr;
	m_vertexInput.flags = 0u;

	m_inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_inputAssembly.pNext = nullptr;
	m_inputAssembly.flags = 0u;
	m_inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_inputAssembly.primitiveRestartEnable = VK_FALSE;

	m_dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_dynamicState.pNext = nullptr;

	// enable all dynamic states, dont bake these into pipeline
	static const VkDynamicState dynamicStates[] =
	{ 
	//	VK_DYNAMIC_STATE_VIEWPORT,
	//	VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH,
		VK_DYNAMIC_STATE_DEPTH_BIAS,
		VK_DYNAMIC_STATE_BLEND_CONSTANTS,
		VK_DYNAMIC_STATE_DEPTH_BOUNDS,
		VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
		VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
		VK_DYNAMIC_STATE_STENCIL_REFERENCE
	};

	m_dynamicState.dynamicStateCount = static_cast<uint32_t>(sizeof(dynamicStates) / sizeof(VkDynamicState));
	m_dynamicState.pDynamicStates = dynamicStates;

	// rasterizer defaults
	// TODO: add setters for rasterizer configuration
	m_rasterState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_rasterState.pNext = nullptr;
	m_rasterState.cullMode = VK_CULL_MODE_NONE;
	m_rasterState.depthBiasClamp = 0.f;
	m_rasterState.depthBiasConstantFactor = 1.f;
	m_rasterState.depthBiasEnable = VK_FALSE;
	m_rasterState.depthBiasSlopeFactor = 1.f;
	m_rasterState.depthClampEnable = VK_FALSE;
	m_rasterState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	m_rasterState.lineWidth = 1.f;
	m_rasterState.rasterizerDiscardEnable = VK_FALSE; //true discards ALL fragments!!!
	m_rasterState.polygonMode = VK_POLYGON_MODE_FILL;

	// disable multisampling
	m_multiSample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	m_multiSample.pNext = nullptr;
	m_multiSample.alphaToCoverageEnable = VK_FALSE;
	m_multiSample.alphaToOneEnable = VK_FALSE;
	m_multiSample.flags = 0u;
	m_multiSample.minSampleShading = 0.f;
	m_multiSample.pSampleMask = nullptr;
	m_multiSample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_multiSample.sampleShadingEnable = VK_FALSE;

	m_depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	m_depthStencilState.depthTestEnable = VK_FALSE;
	m_depthStencilState.depthWriteEnable = VK_FALSE;
	m_depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	m_depthStencilState.depthBoundsTestEnable = VK_FALSE;
	m_depthStencilState.minDepthBounds = 0.0f; // Optional
	m_depthStencilState.maxDepthBounds = 1.0f; // Optional
	m_depthStencilState.stencilTestEnable = VK_FALSE;
	m_depthStencilState.front = {}; // Optional
	m_depthStencilState.back = {}; // Optional

	m_tesselationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	m_tesselationState.pNext = nullptr;
	m_tesselationState.flags = 0u;
	m_tesselationState.patchControlPoints = 0u;
}

void IBLLib::GraphicsPipelineDesc::addShaderStage(VkShaderModule _shaderModule, VkShaderStageFlagBits _stage, const char* _entryPoint, const VkSpecializationInfo* _specInfo)
{
	if (_shaderModule != VK_NULL_HANDLE)
	{
		m_shaderStages.emplace_back();
		VkPipelineShaderStageCreateInfo& info = m_shaderStages.back();

		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.pName = _entryPoint;
		info.stage = _stage;
		info.flags = 0u;
		info.pSpecializationInfo = _specInfo;
		info.module = _shaderModule;
	}
}

void IBLLib::GraphicsPipelineDesc::addVertexAttribute(VkFormat _format, uint32_t _binding, uint32_t _offset, uint32_t _location)
{
	m_vertexAttributes.emplace_back();
	VkVertexInputAttributeDescription& info = m_vertexAttributes.back();

	info.format = _format;
	info.binding = _binding;
	info.offset = _offset;
	info.location = _location == UINT32_MAX ? static_cast<uint32_t>(m_vertexAttributes.size() - 1u) : _location;
}

void IBLLib::GraphicsPipelineDesc::addVertexBinding(uint32_t _binding, uint32_t _stride, VkVertexInputRate _rate)
{
	m_vertexBindings.emplace_back();
	VkVertexInputBindingDescription& info = m_vertexBindings.back();

	info.binding = _binding;
	info.inputRate = _rate;
	info.stride = _stride;
}

void IBLLib::GraphicsPipelineDesc::addColorBlendAttachment(const VkPipelineColorBlendAttachmentState& _attachment, const uint32_t _count)
{
	for (uint32_t i = 0; i < _count; ++i)
	{
		m_colorBlendAttachments.push_back(_attachment);
	}
}


void IBLLib::GraphicsPipelineDesc::setPrimitiveTopology(VkPrimitiveTopology _topology, bool _enablePrimitiveRestart)
{
	m_inputAssembly.primitiveRestartEnable = _enablePrimitiveRestart ? VK_TRUE : VK_FALSE;
	m_inputAssembly.topology = _topology;
}

void  IBLLib::GraphicsPipelineDesc::setRenderPass(VkRenderPass _renderPass)
{
	m_info.renderPass = _renderPass;
}

void  IBLLib::GraphicsPipelineDesc::setPipelineLayout(VkPipelineLayout _pipelineLayout)
{
	m_info.layout = _pipelineLayout;
}

void IBLLib::GraphicsPipelineDesc::setViewportExtent(VkExtent2D _extent)
{
	m_viewport.x = 0.0f;
	m_viewport.y = 0.0f;
	m_viewport.width = (float)_extent.width;
	m_viewport.height = (float)_extent.height;
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;

	m_viewportScissor.offset = { 0, 0 };
	m_viewportScissor.extent = _extent;
}

const VkGraphicsPipelineCreateInfo* IBLLib::GraphicsPipelineDesc::getInfo()
{
	// finalize info with dynamic data
	m_viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	m_viewportState.flags = 0;
	m_viewportState.pNext = NULL;
	m_viewportState.viewportCount = 1;
	m_viewportState.pViewports = &m_viewport;
	m_viewportState.scissorCount = 1;
	m_viewportState.pScissors = &m_viewportScissor;

	//ToDo: coupled attachmentCount 

	m_colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_colorBlendState.logicOpEnable = VK_FALSE;
	m_colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	m_colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachments.size());
	m_colorBlendState.pAttachments = m_colorBlendAttachments.data();
	m_colorBlendState.blendConstants[0] = 0.0f;
	m_colorBlendState.blendConstants[1] = 0.0f;
	m_colorBlendState.blendConstants[2] = 0.0f;
	m_colorBlendState.blendConstants[3] = 0.0f;

	m_info.pVertexInputState = &m_vertexInput;
	m_info.pInputAssemblyState = &m_inputAssembly;
	m_info.pRasterizationState = &m_rasterState;
	m_info.pMultisampleState = &m_multiSample;
	m_info.pColorBlendState = &m_colorBlendState;
	m_info.pDynamicState = &m_dynamicState;
	m_info.pViewportState = &m_viewportState;
	m_info.pDepthStencilState = &m_depthStencilState;
	m_info.pTessellationState = &m_tesselationState;

	m_info.stageCount = static_cast<uint32_t>(m_shaderStages.size());
	m_info.pStages = m_shaderStages.data();

	// vertex stage
	m_vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size());
	m_vertexInput.pVertexAttributeDescriptions = m_vertexAttributes.data();
	
	m_vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindings.size());
	m_vertexInput.pVertexBindingDescriptions = m_vertexBindings.data();

	return &m_info;
}

IBLLib::RenderPassDesc::RenderPassDesc()
{
	m_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	m_info.pNext = nullptr;
	m_info.flags = 0u;

	m_subpass.flags = 0u;
	m_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	m_info.pSubpasses = &m_subpass;
	m_info.subpassCount = 1u;
}

void IBLLib::RenderPassDesc::addAttachment(VkFormat _format, VkAttachmentLoadOp _loadOp, VkAttachmentStoreOp _storeOp, VkSampleCountFlagBits _sampleCount, VkImageLayout _initialLayout, VkImageLayout _finalLayout, VkAttachmentLoadOp _stencilLoadOp, VkAttachmentStoreOp _stencilStoreOp)
{
	m_attachments.emplace_back();
	VkAttachmentDescription& info = m_attachments.back();

	info.flags = 0u;
	info.format = _format;
	info.loadOp = _loadOp;
	info.storeOp = _storeOp;
	info.samples = _sampleCount;
	info.initialLayout = _initialLayout;
	info.finalLayout = _finalLayout;
	info.stencilLoadOp = _stencilLoadOp;
	info.stencilStoreOp = _stencilStoreOp;

	m_attachmentRefs.emplace_back();
	VkAttachmentReference& ref = m_attachmentRefs.back();

	ref.attachment = static_cast<uint32_t> (m_attachmentRefs.size() - 1u);
	ref.layout = _finalLayout;
}

const VkRenderPassCreateInfo* IBLLib::RenderPassDesc::getInfo()
{
	m_info.pAttachments = m_attachments.data();
	m_info.attachmentCount = static_cast<uint32_t>(m_attachments.size());

	m_subpass.colorAttachmentCount = static_cast<uint32_t>(m_attachmentRefs.size());
	m_subpass.pColorAttachments = m_attachmentRefs.data();

	return &m_info;
}