#pragma once

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"

#include <cassert>
#include <map>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <utility>
#include <vector>


VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT       messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT              messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
	void*                                        pUserData
) {
#if !defined(NDEBUG)
	if (pCallbackData->messageIdNumber == 648835635) {
		// UNASSIGNED-khronos-Validation-debug-build-warning-message
		return VK_FALSE;
	}
	if (pCallbackData->messageIdNumber == 767975156) {
		// UNASSIGNED-BestPractices-vkCreateInstance-specialuse-extension
		return VK_FALSE;
	}
#endif

	std::cerr << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) << ": "
	          << vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) << ":\n";
	std::cerr << "\t"
	          << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
	std::cerr << "\t"
	          << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
	std::cerr << "\t"
	          << "message         = <" << pCallbackData->pMessage << ">\n";

	if (0 < pCallbackData->queueLabelCount) {
		std::cerr << "\t"
		          << "Queue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++) {
			std::cerr << "\t\t"
			          << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount) {
		std::cerr << "\t"
		          << "CommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
			std::cerr << "\t\t"
			         << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount) {
		std::cerr << "\t"
		          << "Objects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++) {
			std::cerr << "\t\t"
			          << "Object " << i << "\n";
			std::cerr << "\t\t\t"
			          << "objectType   = "
			          << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
			std::cerr << "\t\t\t"
			          << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName) {
				std::cerr << "\t\t\t"
				          << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}

	return VK_TRUE;
}


std::vector<std::string> getDeviceExtensions() {
	return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

std::vector<std::string> getInstanceExtensions() {
	std::vector<std::string> extensions;
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	extensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
	extensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MIR_KHR)
	extensions.push_back(VK_KHR_MIR_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_VI_NN)
	extensions.push_back(VK_NN_VI_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
	extensions.push_back(VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME);
#endif
	return extensions;
}


uint32_t findMemoryType(
	const vk::PhysicalDeviceMemoryProperties& memoryProperties,
	uint32_t typeBits,
	vk::MemoryPropertyFlags requirementsMask
) {
	auto typeIndex = static_cast<uint32_t>(~0);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask ) == requirementsMask)) {
			typeIndex = i;
			break;
		}

		typeBits >>= 1;
	}

	assert(typeIndex != static_cast<uint32_t>(~0));

	return typeIndex;
}

vk::raii::DeviceMemory allocateDeviceMemory(
	const vk::raii::Device&                   device,
	const vk::PhysicalDeviceMemoryProperties& memoryProperties,
	const vk::MemoryRequirements&             memoryRequirements,
	vk::MemoryPropertyFlags                   memoryPropertyFlags
) {
	const uint32_t memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);
	const auto memoryAllocateInfo = vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex);
	return vk::raii::DeviceMemory(device, memoryAllocateInfo);
}

std::vector<const char*> gatherLayers(
	const std::vector<std::string>& layers
#if !defined(NDEBUG)
	,
	const std::vector<vk::LayerProperties>& layerProperties
#endif
) {
	std::vector<const char*> enabledLayers;
	enabledLayers.reserve(layers.size());

	for (const auto& layer : layers) {
		assert(
			std::ranges::any_of(layerProperties, [layer](const vk::LayerProperties& lp) {
				return layer == lp.layerName;
			})
		);

		enabledLayers.push_back(layer.data());
	}

#if !defined(NDEBUG)
	// Enable standard validation layer to find as much errors as possible!
	const bool has_validation_layer = std::ranges::find(layers, "VK_LAYER_KHRONOS_validation") != layers.end();

	const bool validation_layer_exists = std::ranges::any_of(layerProperties, [](const vk::LayerProperties& lp) {
		return (strcmp("VK_LAYER_KHRONOS_validation", lp.layerName) == 0);
	});

	if (!has_validation_layer && validation_layer_exists) {
		enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
	}
#endif

	return enabledLayers;
}

std::vector<const char*> gatherExtensions(
	const std::vector<std::string>& extensions
#if !defined(NDEBUG)
	,
	const std::vector<vk::ExtensionProperties>& extensionProperties
#endif
) {
	std::vector<const char*> enabledExtensions;
	enabledExtensions.reserve(extensions.size());

	for (const auto& ext : extensions) {
		assert(
			std::ranges::any_of(extensionProperties, [ext](const vk::ExtensionProperties& ep) {
				return ext == ep.extensionName;
			})
		);

		enabledExtensions.push_back(ext.data());
	}

#if !defined(NDEBUG)
	const bool has_debug_utils = std::ranges::find(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != extensions.end();

	const bool debug_utils_exist = std::ranges::any_of(extensionProperties, [](const vk::ExtensionProperties& ep) {
		return (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, ep.extensionName) == 0);
	});

	if (!has_debug_utils && debug_utils_exist) {
		enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
#endif

	return enabledExtensions;
}

uint32_t findGraphicsQueueFamilyIndex(const std::vector<vk::QueueFamilyProperties>& queue_family_properties) {
	// Get the first index into queueFamiliyProperties which supports graphics
	const auto graphics_queue_family_property = std::ranges::find_if(queue_family_properties, [](const vk::QueueFamilyProperties& qfp) {
		return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
	});

	assert(graphics_queue_family_property != queue_family_properties.end());

	return static_cast<uint32_t>(std::distance(queue_family_properties.begin(), graphics_queue_family_property));
}

std::pair<uint32_t, uint32_t> findGraphicsAndPresentQueueFamilyIndex(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface) {
	std::vector<vk::QueueFamilyProperties> queue_family_properties = physical_device.getQueueFamilyProperties();

	assert(queue_family_properties.size() < std::numeric_limits<uint32_t>::max());

	const uint32_t graphics_queue_family_index = findGraphicsQueueFamilyIndex(queue_family_properties);
	if (physical_device.getSurfaceSupportKHR(graphics_queue_family_index, *surface)) {
		// The first graphics_queue_family_index also supports present
		return std::make_pair(graphics_queue_family_index, graphics_queue_family_index);
	}

	// The graphics_queue_family_index doesn't support present
	// Look for an other family index that supports both graphics and present
	for (size_t i = 0; i < queue_family_properties.size(); ++i) {
		if ((queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
			return std::make_pair(static_cast<uint32_t>(i), static_cast<uint32_t>(i));
		}
	}

	// there's nothing like a single family index that supports both graphics and present -> look for an other
	// family index that supports present
	for (size_t i = 0; i < queue_family_properties.size(); ++i) {
		if (physical_device.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)) {
			return std::make_pair(graphics_queue_family_index, static_cast<uint32_t>(i));
		}
	}

	throw std::runtime_error("Could not find both graphics and present queues");
}

auto makeInstanceCreateInfoChain(
	const vk::ApplicationInfo& applicationInfo,
	const std::vector<const char*>& layers,
	const std::vector<const char*>& extensions
) {
	// Return type depends on debug/release build
#if defined(NDEBUG)
	using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo>;
#else
	using instance_create_chain = vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT>;
#endif

#if defined(NDEBUG)
	// In release mode just use the InstanceCreateInfo for instance creation
	const auto instanceCreateInfo = instance_create_chain(
		{{}, &applicationInfo, enabledLayers, enabledExtensions}
	);
#else
	// In debug mode, addionally use the debugUtilsMessengerCallback in instance creation
	const auto severityFlags = vk::DebugUtilsMessageSeverityFlagsEXT(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	const auto messageTypeFlags = vk::DebugUtilsMessageTypeFlagsEXT(
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral     |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
	);

	const auto instanceCreateInfo = instance_create_chain(
		{{}, &applicationInfo, layers, extensions},
		{{}, severityFlags, messageTypeFlags, &debugUtilsMessengerCallback}
	);
#endif

	return instanceCreateInfo;
}

std::unique_ptr<vk::raii::Instance> makeInstance(
	const vk::raii::Context& context,
	const std::string& app_name,
	const std::string& engine_name,
	const std::vector<std::string>& layers = {},
	const std::vector<std::string>& extensions = {},
	uint32_t api_version = VK_API_VERSION_1_0
) {
	const auto applicationInfo = vk::ApplicationInfo(app_name.c_str(), 1, engine_name.c_str(), 1, api_version);
	std::vector<const char*> enabledLayers = gatherLayers(layers, context.enumerateInstanceLayerProperties());
	std::vector<const char*> enabledExtensions = gatherExtensions(extensions, context.enumerateInstanceExtensionProperties());

	const auto instanceCreateInfoChain = makeInstanceCreateInfoChain(applicationInfo, enabledLayers, enabledExtensions);

	return std::make_unique<vk::raii::Instance>(context, instanceCreateInfoChain.get<vk::InstanceCreateInfo>());
}

std::unique_ptr<vk::raii::PhysicalDevice> makePhysicalDevice(const vk::raii::Instance& instance) {
	return std::make_unique<vk::raii::PhysicalDevice>(
		std::move(vk::raii::PhysicalDevices(instance).front())
	);
}

std::unique_ptr<vk::raii::Device> makeDevice(
	const vk::raii::PhysicalDevice&   physicalDevice,
	uint32_t                          queueFamilyIndex,
	const std::vector<std::string>&   extensions             = {},
	const vk::PhysicalDeviceFeatures* physicalDeviceFeatures = nullptr,
	const void*                       pNext                  = nullptr
) {
	std::vector<const char*> enabledExtensions;
	enabledExtensions.reserve(extensions.size());

	for (const auto& ext : extensions) {
		enabledExtensions.push_back(ext.data());
	}

	float queuePriority = 0.0f;
	auto deviceQueueCreateInfo = vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority);
	auto deviceCreateInfo = vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions, physicalDeviceFeatures);
	deviceCreateInfo.pNext = pNext;

	return std::make_unique<vk::raii::Device>(physicalDevice, deviceCreateInfo);
}

std::unique_ptr<vk::raii::DescriptorPool> makeDescriptorPool(const vk::raii::Device& device, const std::vector<vk::DescriptorPoolSize>& pool_sizes)
{
	assert(!pool_sizes.empty());

	const uint32_t max_sets = std::accumulate(
		pool_sizes.begin(),
		pool_sizes.end(),
		0,
		[](uint32_t sum, const  vk::DescriptorPoolSize& dps) {
			return sum + dps.descriptorCount;
		}
	);
	assert(max_sets > 0);

	const auto descriptor_pool_create_info = vk::DescriptorPoolCreateInfo(
		vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		max_sets,
		pool_sizes
	);
	return std::make_unique<vk::raii::DescriptorPool>(device, descriptor_pool_create_info);
}

std::unique_ptr<vk::raii::DescriptorSet> makeDescriptorSet(
	const vk::raii::Device&              device,
	const vk::raii::DescriptorPool&      descriptor_pool,
	const vk::raii::DescriptorSetLayout& descriptor_set_layout
) {
	const auto descriptor_set_allocate_info = vk::DescriptorSetAllocateInfo(*descriptor_pool, *descriptor_set_layout);

	return std::make_unique<vk::raii::DescriptorSet>(
		std::move(vk::raii::DescriptorSets(device, descriptor_set_allocate_info).front())
	);
}

std::unique_ptr<vk::raii::DescriptorSetLayout> makeDescriptorSetLayout(
	const vk::raii::Device& device,
	const std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>& binding_data,
	vk::DescriptorSetLayoutCreateFlags flags = {}
) {
	auto bindings = std::vector<vk::DescriptorSetLayoutBinding>(binding_data.size());

	for (size_t i = 0; i < binding_data.size(); i++) {
		bindings[i] = vk::DescriptorSetLayoutBinding(
			vk::su::checked_cast<uint32_t>( i ),
			std::get<0>(binding_data[i]),
			std::get<1>(binding_data[i]),
			std::get<2>(binding_data[i])
		);
	}

	const auto descriptor_set_layout_create_info = vk::DescriptorSetLayoutCreateInfo(flags, bindings);
	return std::make_unique<vk::raii::DescriptorSetLayout>(device, descriptor_set_layout_create_info);
}

std::unique_ptr<vk::raii::PipelineLayout> makePipelineLayout(const vk::raii::Device& device, const vk::raii::DescriptorSetLayout& descriptor_set_layout) {
	const auto pipeline_layout_create_info = vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), *descriptor_set_layout);
	return std::make_unique<vk::raii::PipelineLayout>(device, pipeline_layout_create_info);
}

std::unique_ptr<vk::raii::RenderPass> makeRenderPass(
	const vk::raii::Device&  device,
	vk::Format               color_format,
	vk::Format               depth_format,
	vk::AttachmentLoadOp     load_op            = vk::AttachmentLoadOp::eClear,
	vk::ImageLayout          color_final_layout = vk::ImageLayout::ePresentSrcKHR
) {
	assert(color_format != vk::Format::eUndefined);
	std::vector<vk::AttachmentDescription> attachment_descriptions;

	attachment_descriptions.emplace_back(
		vk::AttachmentDescriptionFlags(),
		color_format,
		vk::SampleCountFlagBits::e1,
		load_op,
		vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		color_final_layout
	);

	if (depth_format != vk::Format::eUndefined) {
		attachment_descriptions.emplace_back(
			vk::AttachmentDescriptionFlags(),
			depth_format,
			vk::SampleCountFlagBits::e1,
			load_op,
			vk::AttachmentStoreOp::eDontCare,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal
		);
	}

	auto color_attachment = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	auto depth_attachment = vk::AttachmentReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	auto subpass_description = vk::SubpassDescription(
		vk::SubpassDescriptionFlags(),
		vk::PipelineBindPoint::eGraphics,
		{},
		color_attachment,
		{},
		(depth_format != vk::Format::eUndefined) ? &depth_attachment : nullptr
	);

	const auto render_pass_create_info = vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), attachment_descriptions, subpass_description);
	return vk::raii::su::make_unique<vk::raii::RenderPass>(device, render_pass_create_info);
}


std::vector<std::unique_ptr<vk::raii::Framebuffer>> makeFramebuffers(
	const vk::raii::Device&                     device,
	vk::raii::RenderPass &                      render_pass,
	const std::vector<vk::raii::ImageView>&     image_views,
	const std::unique_ptr<vk::raii::ImageView>& depth_image_view,
	const vk::Extent2D&                         extent
) {
	vk::ImageView attachments[2];
	attachments[1] = depth_image_view ? **depth_image_view : vk::ImageView();

	const auto framebuffer_create_info = vk::FramebufferCreateInfo(
		vk::FramebufferCreateFlags(),
		*render_pass,
		depth_image_view ? 2 : 1,
		attachments,
		extent.width,
		extent.height,
		1
	);

	std::vector<std::unique_ptr<vk::raii::Framebuffer>> framebuffers;
	framebuffers.reserve(image_views.size());
	for (const auto& image_view : image_views)
	{
		attachments[0] = *image_view;
		framebuffers.push_back(std::make_unique<vk::raii::Framebuffer>(device, framebuffer_create_info));
	}

	return framebuffers;
}

std::unique_ptr<vk::raii::Pipeline> makeGraphicsPipeline(
	const vk::raii::Device&                             device,
	const vk::raii::PipelineCache&                      pipeline_cache,
	const vk::raii::ShaderModule&                       vertex_shader_module,
	const vk::SpecializationInfo*                       vertex_shader_specialization_info,
	const vk::raii::ShaderModule&                       fragment_shader_module,
	const vk::SpecializationInfo*                       fragment_shader_specialization_info,
	uint32_t                                            vertex_stride,
	const std::vector<std::pair<vk::Format, uint32_t>>& vertex_input_attribute_format_offset,
	vk::FrontFace                                       front_face,
	bool                                                depth_buffered,
	const vk::raii::PipelineLayout&                     pipeline_layout,
	const vk::raii::RenderPass&                         render_pass
) {
	auto pipeline_shader_stage_create_infos = std::array<vk::PipelineShaderStageCreateInfo, 2>{
		vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, *vertex_shader_module, "main", vertex_shader_specialization_info),
		vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eFragment, *fragment_shader_module, "main", fragment_shader_specialization_info)
	};

	auto vertex_input_attribute_descriptions     = std::vector<vk::VertexInputAttributeDescription>{};
	auto pipeline_vertex_input_state_create_info = vk::PipelineVertexInputStateCreateInfo{};
	const auto vertex_input_binding_description  = vk::VertexInputBindingDescription(0, vertex_stride);

	if (vertex_stride > 0) {
		vertex_input_attribute_descriptions.reserve(vertex_input_attribute_format_offset.size());

		for (uint32_t i = 0; i < vertex_input_attribute_format_offset.size(); i++) {
			vertex_input_attribute_descriptions.emplace_back(
				i,
				0,
				vertex_input_attribute_format_offset[i].first,
				vertex_input_attribute_format_offset[i].second
			);
		}
	
		pipeline_vertex_input_state_create_info.setVertexBindingDescriptions(vertex_input_binding_description);
		pipeline_vertex_input_state_create_info.setVertexAttributeDescriptions(vertex_input_attribute_descriptions);
	}

	const auto pipeline_input_assembly_state_create_info = vk::PipelineInputAssemblyStateCreateInfo(
		vk::PipelineInputAssemblyStateCreateFlags(),
		vk::PrimitiveTopology::eTriangleList
	);

	const auto pipeline_viewport_state_create_info = vk::PipelineViewportStateCreateInfo(
		vk::PipelineViewportStateCreateFlags(),
		1,
		nullptr,
		1,
		nullptr
	);

	const auto pipelineRasterizationStateCreateInfo = vk::PipelineRasterizationStateCreateInfo(
		vk::PipelineRasterizationStateCreateFlags(),
		false,
		false,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack,
		front_face,
		false,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	);

	const auto pipelineMultisampleStateCreateInfo = vk::PipelineMultisampleStateCreateInfo({}, vk::SampleCountFlagBits::e1);

	const auto stencil_op_state = vk::StencilOpState(
		vk::StencilOp::eKeep,
		vk::StencilOp::eKeep,
		vk::StencilOp::eKeep,
		vk::CompareOp::eAlways
	);

	const auto pipeline_depth_stencil_state_create_info = vk::PipelineDepthStencilStateCreateInfo(
		vk::PipelineDepthStencilStateCreateFlags(),
		depth_buffered,
		depth_buffered,
		vk::CompareOp::eLessOrEqual,
		false,
		false,
		stencil_op_state,
		stencil_op_state
	);

	const auto color_component_flags = vk::ColorComponentFlags(
		vk::ColorComponentFlagBits::eR |
		vk::ColorComponentFlagBits::eG |
		vk::ColorComponentFlagBits::eB |
		vk::ColorComponentFlagBits::eA
	);
	const auto pipeline_color_blend_attachment_state = vk::PipelineColorBlendAttachmentState(
		false,
		vk::BlendFactor::eZero,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eZero,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		color_component_flags
	);

	const auto pipeline_color_blend_state_create_info = vk::PipelineColorBlendStateCreateInfo(
		vk::PipelineColorBlendStateCreateFlags(),
		false,
		vk::LogicOp::eNoOp,
		pipeline_color_blend_attachment_state,
		{{1.0f, 1.0f, 1.0f, 1.0f}}
	);

	const auto dynamic_states = std::array<vk::DynamicState, 2>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
	const auto pipeline_dynamic_state_create_info = vk::PipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamic_states);

	const auto graphics_pipeline_create_info = vk::GraphicsPipelineCreateInfo(
		vk::PipelineCreateFlags(),
		pipeline_shader_stage_create_infos,
		&pipeline_vertex_input_state_create_info,
		&pipeline_input_assembly_state_create_info,
		nullptr,
		&pipeline_viewport_state_create_info,
		&pipelineRasterizationStateCreateInfo,
		&pipelineMultisampleStateCreateInfo,
		&pipeline_depth_stencil_state_create_info,
		&pipeline_color_blend_state_create_info,
		&pipeline_dynamic_state_create_info,
		*pipeline_layout,
		*render_pass
	);

	return std::make_unique<vk::raii::Pipeline>(device, pipeline_cache, graphics_pipeline_create_info);
}

void updateDescriptorSets(
	const vk::raii::Device& device,
	const vk::raii::DescriptorSet& descriptor_set,
	const std::vector<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, const vk::raii::BufferView*>>& buffer_data,
	const vk::raii::su::TextureData& texture_data,
	uint32_t binding_offset = 0
) {
	std::vector<vk::DescriptorBufferInfo> buffer_infos;
	buffer_infos.reserve(buffer_data.size());

	std::vector<vk::WriteDescriptorSet> write_descriptor_sets;
	write_descriptor_sets.reserve(buffer_data.size() + 1);
	uint32_t dst_binding = binding_offset;
	for (const auto& bhd : buffer_data)	{
		buffer_infos.emplace_back(*std::get<1>(bhd), 0, VK_WHOLE_SIZE);

		vk::BufferView buffer_view;
		if (std::get<2>(bhd)) {
			buffer_view = **std::get<2>(bhd);
		}

		write_descriptor_sets.emplace_back(
			*descriptor_set,
			dst_binding++,
			0,
			1,
			std::get<0>(bhd),
			nullptr,
			&buffer_infos.back(),
			std::get<2>(bhd) ? &buffer_view : nullptr
		);
	}

	const auto image_info = vk::DescriptorImageInfo(
		**texture_data.sampler,
		**texture_data.imageData->imageView,
		vk::ImageLayout::eShaderReadOnlyOptimal
	);

	write_descriptor_sets.emplace_back(
		*descriptor_set,
		dst_binding,
		0,
		vk::DescriptorType::eCombinedImageSampler,
		image_info,
		nullptr,
		nullptr
	);

	device.updateDescriptorSets(write_descriptor_sets, nullptr);
}

void updateDescriptorSets(
	const vk::raii::Device& device,
	const vk::raii::DescriptorSet& descriptor_set,
	const std::vector<std::tuple<vk::DescriptorType, const vk::raii::Buffer&, const vk::raii::BufferView*>>& buffer_data,
	const std::vector<vk::raii::su::TextureData>& texture_data,
	uint32_t binding_offset = 0
) {
	std::vector<vk::DescriptorBufferInfo> buffer_infos;
	buffer_infos.reserve(buffer_data.size());

	std::vector<vk::WriteDescriptorSet> write_descriptor_sets;
	write_descriptor_sets.reserve(buffer_data.size() + (texture_data.empty() ? 0 : 1));
	uint32_t dst_binding = binding_offset;
	for (const auto& bhd : buffer_data)	{
		buffer_infos.emplace_back(*std::get<1>(bhd), 0, VK_WHOLE_SIZE);
	
		vk::BufferView buffer_view;
		if (std::get<2>(bhd)) {
			buffer_view = **std::get<2>(bhd);
		}

		write_descriptor_sets.emplace_back(
			*descriptor_set,
			dst_binding++,
			0,
			1,
			std::get<0>(bhd),
			nullptr,
			&buffer_infos.back(),
			std::get<2>(bhd) ? &buffer_view : nullptr
		);
	}

	std::vector<vk::DescriptorImageInfo> image_infos;
	if (!texture_data.empty()) {
		image_infos.reserve(texture_data.size());

		for (const auto& thd : texture_data) {
			image_infos.emplace_back(**thd.sampler, **thd.imageData->imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		}

		write_descriptor_sets.emplace_back(
			*descriptor_set,
			dst_binding,
			0,
			vk::su::checked_cast<uint32_t>(image_infos.size()),
			vk::DescriptorType::eCombinedImageSampler,
			image_infos.data(),
			nullptr,
			nullptr
		);
	}

	device.updateDescriptorSets(write_descriptor_sets, nullptr);
}
