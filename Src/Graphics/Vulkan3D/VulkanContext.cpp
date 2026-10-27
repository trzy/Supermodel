/**
 ** Supermodel-VK
 ** A Vulkan-focused fork of Supermodel, the Sega Model 3 arcade emulator.
 ** Copyright 2026 The Supermodel-VK authors
 **
 ** This file is part of Supermodel-VK, and is derived from Supermodel,
 ** Copyright 2003-2026 The Supermodel Team.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/
#include "VulkanContext.h"

#include "OSD/Logger.h"
#include <SDL.h>
#include <SDL_vulkan.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace Vulkan3D {

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT,
	const VkDebugUtilsMessengerCallbackDataEXT* data,
	void*)
{
	if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		ErrorLog("Vulkan: %s", data->pMessage);
	else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		ErrorLog("Vulkan warning: %s", data->pMessage);
	return VK_FALSE;
}

/*
 * Returns false for Vulkan devices that are not real GPU drivers and would only clutter the
 * device list:
 *
 *  - software rasterisers (VK_PHYSICAL_DEVICE_TYPE_CPU), such as "Microsoft Basic Render Driver"
 *  - the Direct3D 12 mapping layer, which re-exports every physical GPU under a name like
 *    "Microsoft Direct3D12 (NVIDIA GeForce RTX 5060 Ti)". These are duplicates of devices
 *    that already appear under their real driver.
 */
static bool IsDisplayDevice(const VkPhysicalDeviceProperties& props)
{
	if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
		return false;

	static const char d3d12DriverPrefix[] = "Microsoft Direct3D12";
	if (std::strncmp(props.deviceName, d3d12DriverPrefix, sizeof(d3d12DriverPrefix) - 1) == 0)
		return false;

	return true;
}

static const char* PresentModeName(VkPresentModeKHR mode)
{
	switch (mode)
	{
	case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "IMMEDIATE (no vsync)";
	case VK_PRESENT_MODE_MAILBOX_KHR:      return "MAILBOX (vsync off, latest frame)";
	case VK_PRESENT_MODE_FIFO_KHR:         return "FIFO (vsync on)";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
	default:                               return "other";
	}
}

CVulkanContext::CVulkanContext() = default;

CVulkanContext::~CVulkanContext()
{
	Shutdown();
}

VkCommandBuffer CVulkanContext::Cmd() const
{
	return m_frames[m_frame].cmd;
}

VkImage CVulkanContext::SwapImage() const
{
	return m_imageIndex < m_images.size() ? m_images[m_imageIndex].image : VK_NULL_HANDLE;
}

VkImageView CVulkanContext::SwapView() const
{
	return m_imageIndex < m_images.size() ? m_images[m_imageIndex].view : VK_NULL_HANDLE;
}

Result CVulkanContext::InitDevice(SDL_Window* window, bool validation, bool vsync, const std::string& deviceName)
{
	m_window = window;
	m_validation = validation;
	m_vsync = vsync;
	m_devicePreference = deviceName;

	if (volkInitialize() != VK_SUCCESS)
	{
#ifdef __APPLE__
		ErrorLog("Vulkan: no Vulkan implementation found. Install MoltenVK with "
			"'brew install molten-vk' or the Vulkan SDK, or choose the New 3D renderer.");
#else
		ErrorLog("Vulkan: volkInitialize failed. Is vulkan-1.dll installed?");
#endif
		return Result::FAIL;
	}
	if (!CreateInstance())
		return Result::FAIL;
	volkLoadInstance(m_instance);

	if (!SDL_Vulkan_CreateSurface(window, m_instance, &m_surface))
	{
		ErrorLog("Vulkan: SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		return Result::FAIL;
	}
	if (!PickDevice() || !CreateDevice())
		return Result::FAIL;
	volkLoadDevice(m_device);

	/*
	 * The entry points the renderer reaches through the wrappers in VulkanCommon.h have to exist by
	 * now, and this is the only place that can tell.
	 *
	 * volk fills the core name on a 1.3 device and the VK_KHR_ name on a 1.2 device with the extension
	 * enabled, but it fills neither for a device that has neither - and the wrappers are written to do
	 * nothing rather than crash when both are null. That is the right behaviour for a draw command and
	 * the wrong thing to leave undetected, so it is checked once here, where the failure can still be
	 * reported as a device problem instead of appearing later as an empty screen.
	 */
	if (!Rendering2EntryPointsAvailable())
	{
		ErrorLog("Vulkan: the device was accepted but none of dynamic rendering or synchronization2 was "
			"loaded; this driver cannot run the backend. The OpenGL renderer (New 3D or Legacy 3D) can be "
			"chosen on the Video tab.");
		return Result::FAIL;
	}

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(m_physical, &props);
	m_uniformAlign = props.limits.minUniformBufferOffsetAlignment;
	m_storageAlign = props.limits.minStorageBufferOffsetAlignment;
	if (m_uniformAlign == 0)
		m_uniformAlign = 256;
	if (m_storageAlign == 0)
		m_storageAlign = 256;

	/*
	 * The one line worth reading first in any log from unfamiliar hardware.
	 *
	 * It states which of the two routes the backend is running through, because the same logic executes
	 * through different entry points depending on it - so a report that names the route narrows down
	 * immediately whether the 1.2 path is involved at all.
	 */
	InfoLog("Vulkan: using %s (API %u.%u, %s).", props.deviceName,
		VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion),
		m_vulkan13 ? "Vulkan 1.3 core path" : "Vulkan 1.2 extension path");

	if (!m_memory.Init(m_physical, m_device))
		return Result::FAIL;
	if (!m_ring.Init(m_memory, kMaxFramesInFlight, kRingSliceBytes))
		return Result::FAIL;
	if (!CreateSwapchain() || !CreateFrames())
		return Result::FAIL;
	return Result::OKAY;
}

void CVulkanContext::Shutdown()
{
	if (m_device)
		vkDeviceWaitIdle(m_device);


	DestroyFrames();
	DestroySwapchain();
	m_ring.Shutdown(m_memory);
	m_memory.Shutdown();
	if (m_device)
	{
		vkDestroyDevice(m_device, nullptr);
		m_device = VK_NULL_HANDLE;
	}
	if (m_surface && m_instance)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}
	if (m_debug && m_instance)
	{
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug, nullptr);
		m_debug = VK_NULL_HANDLE;
	}
	if (m_instance)
	{
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
	m_window = nullptr;
	m_recording = false;
}

bool CVulkanContext::CreateInstance()
{
	unsigned extCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(m_window, &extCount, nullptr))
	{
		ErrorLog("Vulkan: SDL_Vulkan_GetInstanceExtensions failed.");
		return false;
	}
	std::vector<const char*> extensions(extCount);
	SDL_Vulkan_GetInstanceExtensions(m_window, &extCount, extensions.data());

	std::vector<const char*> layers;
	if (m_validation)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		layers.push_back("VK_LAYER_KHRONOS_validation");
	}

	VkApplicationInfo app = {};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "Supermodel";
	app.applicationVersion = VK_MAKE_API_VERSION(0, 0, 3, 0);
	app.pEngineName = "Supermodel";
	app.engineVersion = app.applicationVersion;
	app.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	info.pApplicationInfo = &app;
	info.enabledExtensionCount = (uint32_t)extensions.size();
	info.ppEnabledExtensionNames = extensions.data();
	info.enabledLayerCount = (uint32_t)layers.size();
	info.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();

	VkResult result = vkCreateInstance(&info, nullptr, &m_instance);
	if (result != VK_SUCCESS && m_validation)
	{
		ErrorLog("Vulkan: validation layer unavailable, retrying without it.");
		m_validation = false;
		extensions.pop_back();
		info.enabledExtensionCount = (uint32_t)extensions.size();
		info.ppEnabledExtensionNames = extensions.data();
		info.enabledLayerCount = 0;
		info.ppEnabledLayerNames = nullptr;
		result = vkCreateInstance(&info, nullptr, &m_instance);
	}
	if (result != VK_SUCCESS)
	{
		/*
		 * There is deliberately no retry with a lower apiVersion here any more.
		 *
		 * That retry used to be the entry point to the Vulkan 1.2 path, which could not actually work:
		 * the shaders are compiled to SPIR-V 1.6, which needs Vulkan 1.3 or VK_KHR_maintenance4, and
		 * maintenance4 was never in the required extension list. A machine that reached the 1.2 path
		 * would have failed a few steps later at shader-module creation, with a message that pointed at
		 * the shader rather than at the real problem. Reporting the requirement plainly is more useful.
		 */
		ErrorLog("Vulkan: vkCreateInstance failed (%d) for a Vulkan 1.3 instance. This backend requires a "
			"Vulkan 1.3 driver; the OpenGL renderer (New 3D or Legacy 3D) can be chosen on the Video tab.",
			(int)result);
		return false;
	}

	if (m_validation)
	{
		VkDebugUtilsMessengerCreateInfoEXT dbg = {};
		dbg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbg.pfnUserCallback = DebugCallback;
		// volkLoadInstance has not run yet on the first successful 1.3 path; load now if needed.
		volkLoadInstance(m_instance);
		if (vkCreateDebugUtilsMessengerEXT)
			vkCreateDebugUtilsMessengerEXT(m_instance, &dbg, nullptr, &m_debug);
	}
	return true;
}

/*
 * Whether a device already advertises a given device-level extension.
 *
 * Asked per candidate rather than once for the chosen device, because extension availability is a
 * property of the device and the candidates are being compared before one is chosen.
 */
static bool HasDeviceExtension(VkPhysicalDevice device, const char* name)
{
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
	std::vector<VkExtensionProperties> available(count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
	for (const auto& ext : available)
		if (std::strcmp(ext.extensionName, name) == 0)
			return true;
	return false;
}

/*
 * How a device can run this backend, which decides both whether it is a candidate and how it is set up.
 *
 *   Vulkan13           all three entry points the renderer needs are core, nothing to enable but the
 *                      features themselves.
 *   Vulkan12Extensions Vulkan 1.2 with VK_KHR_dynamic_rendering and VK_KHR_synchronization2 - the
 *                      same two capabilities by their extension names. This is what keeps a whole
 *                      generation of hardware usable: 1.3 was required for what turned out to be two
 *                      optional extensions, and the devices that lost out are the older ones this
 *                      renderer has the most to offer - it is the cheaper path on integrated graphics,
 *                      because it uploads less.
 *   Unsupported        too old, or missing one of the two.
 *
 * The shaders are built for SPIR-V 1.5 so that neither route needs VK_KHR_maintenance4, which is the
 * detail that made an earlier attempt at this fail at shader creation rather than here.
 */
enum class BackendRoute { Unsupported, Vulkan13, Vulkan12Extensions };

static BackendRoute ClassifyDevice(VkPhysicalDevice device, const VkPhysicalDeviceProperties& props)
{
	if (props.apiVersion >= VK_API_VERSION_1_3)
		return BackendRoute::Vulkan13;

	if (props.apiVersion < VK_API_VERSION_1_2)
		return BackendRoute::Unsupported;

	if (HasDeviceExtension(device, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) &&
		HasDeviceExtension(device, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
		return BackendRoute::Vulkan12Extensions;

	return BackendRoute::Unsupported;
}

bool CVulkanContext::PickDevice()
{
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
	if (count == 0)
	{
		ErrorLog("Vulkan: no physical devices.");
		return false;
	}
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

	// Returns the queue family that can both render and present on this device, or UINT32_MAX.
	auto presentFamily = [&](VkPhysicalDevice device) -> uint32_t
	{
		uint32_t qCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &qCount, nullptr);
		std::vector<VkQueueFamilyProperties> queues(qCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &qCount, queues.data());
		for (uint32_t i = 0; i < qCount; i++)
		{
			if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				continue;
			VkBool32 present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present);
			if (present)
				return i;
		}
		return UINT32_MAX;
	};

	// If a specific device was requested in the configuration, honour it when it is usable.
	if (!m_devicePreference.empty())
	{
		for (auto device : devices)
		{
			VkPhysicalDeviceProperties props = {};
			vkGetPhysicalDeviceProperties(device, &props);
			if (m_devicePreference != props.deviceName)
				continue;
			if (props.limits.maxPushConstantsSize < 256)
			{
				ErrorLog("Vulkan: requested device '%s' has insufficient push constant space.", m_devicePreference.c_str());
				break;
			}
			uint32_t family = presentFamily(device);
			if (family == UINT32_MAX)
			{
				ErrorLog("Vulkan: requested device '%s' cannot render to this window.", m_devicePreference.c_str());
				break;
			}
			if (ClassifyDevice(device, props) == BackendRoute::Unsupported)
			{
				ErrorLog("Vulkan: requested device '%s' reports API %u.%u without the extensions the backend "
					"needs (Vulkan 1.3, or 1.2 with dynamic rendering and synchronization2).",
					m_devicePreference.c_str(),
					VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion));
				break;
			}
			m_physical = device;
			m_queueFamily = family;
			return true;
		}
		ErrorLog("Vulkan: device '%s' was not found. Falling back to automatic selection.", m_devicePreference.c_str());
	}

	// Automatic selection skips the devices hidden from the settings window (see
	// IsDisplayDevice()) so that a real driver is preferred over a D3D12 shim. If that
	// leaves nothing usable, every device is considered rather than failing outright.
	int bestScore = -1;
	for (int pass = 0; pass < 2 && m_physical == VK_NULL_HANDLE; pass++)
	{
		const bool includeHidden = (pass == 1);
		bestScore = -1;
		for (auto device : devices)
		{
			VkPhysicalDeviceProperties props = {};
			vkGetPhysicalDeviceProperties(device, &props);
			if (!includeHidden && !IsDisplayDevice(props))
				continue;
			if (props.limits.maxPushConstantsSize < 256)
				continue;

			uint32_t family = presentFamily(device);
			if (family == UINT32_MAX)
				continue;

			/*
			 * A device that cannot run the backend is not a candidate at all rather than merely scoring
			 * lower - letting one through would only move the failure somewhere less obvious. Vulkan 1.3
			 * or 1.2 with the two extensions behind the rendering and submission paths both qualify; see
			 * ClassifyDevice for why the extension route is worth having.
			 */
			if (ClassifyDevice(device, props) == BackendRoute::Unsupported)
				continue;

			int score = 100;
			if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				score += 1000;
			else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
				score += 100;
			if (score > bestScore)
			{
				bestScore = score;
				m_physical = device;
				m_queueFamily = family;
			}
		}
	}
	if (m_physical == VK_NULL_HANDLE)
	{
		ErrorLog("Vulkan: no device offers a usable API version (Vulkan 1.3, or 1.2 with dynamic "
			"rendering and synchronization2) together with graphics, present and 256-byte push constants. "
			"The OpenGL renderer (New 3D or Legacy 3D) can be chosen on the Video tab; a driver update "
			"may also help.");
		return false;
	}
	return true;
}



bool CVulkanContext::CreateDevice()
{
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extCount, nullptr);
	std::vector<VkExtensionProperties> available(extCount);
	vkEnumerateDeviceExtensionProperties(m_physical, nullptr, &extCount, available.data());
	auto hasExt = [&](const char* name)
	{
		for (const auto& ext : available)
			if (std::strcmp(ext.extensionName, name) == 0)
				return true;
		return false;
	};

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(m_physical, &props);
	const BackendRoute route = ClassifyDevice(m_physical, props);
	if (route == BackendRoute::Unsupported)
	{
		// Not reachable through PickDevice, which only keeps devices that classify as supported. Said
		// rather than assumed: a device that slipped through would fail at the first draw, which is a
		// much worse place to find out.
		ErrorLog("Vulkan: the selected device cannot run this backend.");
		return false;
	}
	m_vulkan13 = (route == BackendRoute::Vulkan13);

	std::vector<const char*> extensions;
	extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	/*
	 * On a 1.3 device the extensions behind dynamic rendering and synchronization2 are core, and their
	 * names need not even be advertised - which is why they are not requested there. On 1.2 they are
	 * the only way to get those two capabilities, so they are requested by name.
	 *
	 * descriptor_indexing, maintenance2, create_renderpass_2 and depth_stencil_resolve are core in 1.2
	 * and are likewise not requested. VK_KHR_maintenance4 is not requested either: it exists to allow
	 * SPIR-V 1.6 on a 1.2 device, and the shaders are built for SPIR-V 1.5, so nothing needs it.
	 */
	if (route == BackendRoute::Vulkan12Extensions)
	{
		extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
		extensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
	}

	m_pushDescriptors = hasExt(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	if (m_pushDescriptors)
		extensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);

	/*
	 * The two capabilities the renderer cannot do without are queried and enabled through different
	 * structs depending on the route.
	 *
	 * On 1.3 they are core, so VkPhysicalDeviceVulkan13Features is the right thing to chain. On 1.2
	 * chaining the 1.3 feature struct is not valid - the same two booleans come from the extension
	 * feature structs instead. Reading the wrong one returns zero, and the backend would then reject a
	 * device that is perfectly capable of running it.
	 */
	VkPhysicalDeviceFeatures2 features2 = {};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	VkPhysicalDeviceVulkan13Features features13 = {};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	VkPhysicalDeviceDynamicRenderingFeaturesKHR featuresDyn = {};
	featuresDyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	VkPhysicalDeviceSynchronization2FeaturesKHR featuresSync = {};
	featuresSync.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

	features2.pNext = &features12;
	if (route == BackendRoute::Vulkan13)
	{
		features12.pNext = &features13;
	}
	else
	{
		features12.pNext = &featuresDyn;
		featuresDyn.pNext = &featuresSync;
	}

	vkGetPhysicalDeviceFeatures2(m_physical, &features2);

	if (!features12.descriptorIndexing || !features12.runtimeDescriptorArray ||
		!features12.shaderSampledImageArrayNonUniformIndexing || !features12.descriptorBindingPartiallyBound ||
		!features12.descriptorBindingSampledImageUpdateAfterBind || !features12.descriptorBindingVariableDescriptorCount)
	{
		ErrorLog("Vulkan: descriptor indexing features are missing.");
		return false;
	}
	if (!features12.samplerMirrorClampToEdge)
	{
		ErrorLog("Vulkan: samplerMirrorClampToEdge is required by the texture samplers.");
		return false;
	}

	const bool hasDynamicRendering = (route == BackendRoute::Vulkan13) ? features13.dynamicRendering : featuresDyn.dynamicRendering;
	const bool hasSynchronization2 = (route == BackendRoute::Vulkan13) ? features13.synchronization2 : featuresSync.synchronization2;
	if (!hasDynamicRendering || !hasSynchronization2)
	{
		ErrorLog("Vulkan: dynamic rendering or synchronization2 is missing.");
		return false;
	}

	VkPhysicalDeviceVulkan12Features enable12 = {};
	enable12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	enable12.descriptorIndexing = VK_TRUE;
	enable12.runtimeDescriptorArray = VK_TRUE;
	enable12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	enable12.descriptorBindingPartiallyBound = VK_TRUE;
	enable12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	enable12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	enable12.timelineSemaphore = features12.timelineSemaphore;
	enable12.samplerMirrorClampToEdge = VK_TRUE;

	// Enabled through the same struct that was queried, for the same reason as above.
	VkPhysicalDeviceVulkan13Features enable13 = {};
	enable13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	VkPhysicalDeviceDynamicRenderingFeaturesKHR enableDyn = {};
	enableDyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	VkPhysicalDeviceSynchronization2FeaturesKHR enableSync = {};
	enableSync.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

	if (route == BackendRoute::Vulkan13)
	{
		enable13.dynamicRendering = VK_TRUE;
		enable13.synchronization2 = VK_TRUE;
		enable12.pNext = &enable13;
	}
	else
	{
		enableDyn.dynamicRendering = VK_TRUE;
		enableSync.synchronization2 = VK_TRUE;
		enableDyn.pNext = &enableSync;
		enable12.pNext = &enableDyn;
	}

	VkPhysicalDeviceFeatures enabled = {};
	enabled.samplerAnisotropy = features2.features.samplerAnisotropy;
	enabled.fragmentStoresAndAtomics = features2.features.fragmentStoresAndAtomics;
	enabled.shaderInt16 = features2.features.shaderInt16;
	VkPhysicalDeviceFeatures2 enable2 = {};
	enable2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	enable2.pNext = &enable12;
	enable2.features = enabled;

	float priority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = m_queueFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &priority;

	VkDeviceCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	info.pNext = &enable2;
	info.queueCreateInfoCount = 1;
	info.pQueueCreateInfos = &queueInfo;
	info.enabledExtensionCount = (uint32_t)extensions.size();
	info.ppEnabledExtensionNames = extensions.data();

	if (vkCreateDevice(m_physical, &info, nullptr, &m_device) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: vkCreateDevice failed.");
		return false;
	}
	vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
	return true;
}

bool CVulkanContext::CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR caps = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical, m_surface, &caps);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical, m_surface, &formatCount, formats.data());
	VkSurfaceFormatKHR chosen = formats[0];
	for (const auto& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			chosen = format;
			break;
		}
		if (format.format == VK_FORMAT_R8G8B8A8_UNORM)
			chosen = format;
	}
	m_swapFormat = chosen.format;

	uint32_t modeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &modeCount, nullptr);
	std::vector<VkPresentModeKHR> modes(modeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical, m_surface, &modeCount, modes.data());
	m_presentMode = VK_PRESENT_MODE_FIFO_KHR;
	if (!m_vsync)
	{
		for (auto mode : modes)
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
				m_presentMode = mode;
		if (m_presentMode == VK_PRESENT_MODE_FIFO_KHR)
		{
			for (auto mode : modes)
				if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
					m_presentMode = mode;
		}
	}

	// Report the full set of present modes once. This tells us up front whether
	// MAILBOX / FIFO_RELAXED are available on this GPU+driver, which decides
	// whether the low-latency / anti-judder options are even reachable.
	static bool s_loggedPresentModes = false;
	if (!s_loggedPresentModes)
	{
		s_loggedPresentModes = true;
		std::string list;
		for (auto mode : modes)
		{
			if (!list.empty())
				list += ", ";
			list += PresentModeName(mode);
		}
		InfoLog("Vulkan: available present modes: %s.", list.c_str());
	}

	if (caps.currentExtent.width != UINT32_MAX)
		m_extent = caps.currentExtent;
	else
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(m_window, &w, &h);
		m_extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, (uint32_t)w));
		m_extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, (uint32_t)h));
	}
	if (m_extent.width == 0 || m_extent.height == 0)
		return false;

	uint32_t imageCount = caps.minImageCount + 1;
	// MAILBOX needs a spare image to write into while the previous one is still
	// queued for scan-out, so three is its minimum for a stable image.
	if (m_presentMode == VK_PRESENT_MODE_MAILBOX_KHR && imageCount < 3)
		imageCount = 3;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
		imageCount = caps.maxImageCount;

	for (auto& image : m_images)
	{
		if (image.view)
			vkDestroyImageView(m_device, image.view, nullptr);
		if (image.renderFinished)
			vkDestroySemaphore(m_device, image.renderFinished, nullptr);
		image.view = VK_NULL_HANDLE;
		image.renderFinished = VK_NULL_HANDLE;
	}
	m_images.clear();
	m_imageSeen.clear();

	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = m_surface;
	info.minImageCount = imageCount;
	info.imageFormat = m_swapFormat;
	info.imageColorSpace = chosen.colorSpace;
	info.imageExtent = m_extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if ((caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0)
		info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	info.presentMode = m_presentMode;
	info.clipped = VK_TRUE;
	info.oldSwapchain = m_swapchain;

	VkSwapchainKHR fresh = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(m_device, &info, nullptr, &fresh) != VK_SUCCESS)
	{
		ErrorLog("Vulkan: vkCreateSwapchainKHR failed.");
		return false;
	}
	if (m_swapchain)
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	m_swapchain = fresh;

	uint32_t got = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &got, nullptr);
	std::vector<VkImage> images(got);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &got, images.data());

	m_images.resize(got);
	m_imageSeen.assign(got, 0);
	for (uint32_t i = 0; i < got; i++)
	{
		m_images[i].image = images[i];
		m_images[i].view = CreateView(images[i], m_swapFormat, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1);
		VkSemaphoreCreateInfo sem = {};
		sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		vkCreateSemaphore(m_device, &sem, nullptr, &m_images[i].renderFinished);
	}
	m_generation++;
	InfoLog("Vulkan: swapchain %ux%u, %u images, present mode %s.",
		m_extent.width, m_extent.height, got, PresentModeName(m_presentMode));
	return true;
}

void CVulkanContext::DestroySwapchain()
{
	if (!m_device)
		return;
	for (auto& image : m_images)
	{
		if (image.view)
			vkDestroyImageView(m_device, image.view, nullptr);
		if (image.renderFinished)
			vkDestroySemaphore(m_device, image.renderFinished, nullptr);
	}
	m_images.clear();
	m_imageSeen.clear();
	if (m_swapchain)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
		m_swapchain = VK_NULL_HANDLE;
	}
}

bool CVulkanContext::CreateFrames()
{
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = m_queueFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VkSemaphoreCreateInfo semInfo = {};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
	{
		if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_frames[i].pool) != VK_SUCCESS)
			return false;
		VkCommandBufferAllocateInfo alloc = {};
		alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc.commandPool = m_frames[i].pool;
		alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(m_device, &alloc, &m_frames[i].cmd) != VK_SUCCESS)
			return false;
		vkCreateFence(m_device, &fenceInfo, nullptr, &m_frames[i].fence);
		vkCreateSemaphore(m_device, &semInfo, nullptr, &m_frames[i].imageAvailable);
	}
	return true;
}

void CVulkanContext::DestroyFrames()
{
	if (!m_device)
		return;
	for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
	{
		if (m_frames[i].pool)
			vkDestroyCommandPool(m_device, m_frames[i].pool, nullptr);
		if (m_frames[i].fence)
			vkDestroyFence(m_device, m_frames[i].fence, nullptr);
		if (m_frames[i].imageAvailable)
			vkDestroySemaphore(m_device, m_frames[i].imageAvailable, nullptr);
		m_frames[i] = Frame();
	}
}

bool CVulkanContext::RecreateSwapchain()
{
	if (!m_device)
		return false;
	vkDeviceWaitIdle(m_device);
	int w = 0, h = 0;
	SDL_GetWindowSize(m_window, &w, &h);
	if (w == 0 || h == 0)
		return false;
	return CreateSwapchain();
}

void CVulkanContext::MarkSwapchainReady()
{
	m_swapReady = true;
	if (m_imageIndex < m_imageSeen.size())
		m_imageSeen[m_imageIndex] = 1;
}

bool CVulkanContext::BeginFrame()
{
	if (!m_device)
		return false;
	Frame& frame = m_frames[m_frame];
	vkWaitForFences(m_device, 1, &frame.fence, VK_TRUE, UINT64_MAX);


	int w = 0, h = 0;
	SDL_GetWindowSize(m_window, &w, &h);
	if (w == 0 || h == 0)
		return false;

	VkResult acquired = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &m_imageIndex);
	if (acquired == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return false;
	}
	if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
	{
		ErrorLog("Vulkan: vkAcquireNextImageKHR failed (%d).", (int)acquired);
		return false;
	}

	vkResetFences(m_device, 1, &frame.fence);
	vkResetCommandPool(m_device, frame.pool, 0);
	VkCommandBufferBeginInfo begin = {};
	begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(frame.cmd, &begin);
	m_ring.BeginFrame(m_frame);
	m_recording = true;
	m_swapReady = false;
	if (acquired == VK_SUBOPTIMAL_KHR)
		m_generation = m_generation; // recreated after present
	return true;
}







void CVulkanContext::EndFrame()
{
	if (!m_recording)
		return;
	Frame& frame = m_frames[m_frame];
	if (!m_swapReady)
	{
		// Acquired a swapchain image but the 2D compositor did not run.
		// Present still requires PRESENT_SRC. After the first present the
		// image comes back in that layout; a brand new image is UNDEFINED.
		VkImageLayout oldLayout = (m_imageIndex < m_imageSeen.size() && m_imageSeen[m_imageIndex])
			? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			: VK_IMAGE_LAYOUT_UNDEFINED;
		CmdTransition(frame.cmd, m_images[m_imageIndex].image, oldLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
		if (m_imageIndex < m_imageSeen.size())
			m_imageSeen[m_imageIndex] = 1;
		m_swapReady = true;
	}
	vkEndCommandBuffer(frame.cmd);

	VkCommandBufferSubmitInfo cmdInfo = {};
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdInfo.commandBuffer = frame.cmd;

	VkSemaphoreSubmitInfo waitInfo = {};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitInfo.semaphore = frame.imageAvailable;
	waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalInfo = {};
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfo.semaphore = m_images[m_imageIndex].renderFinished;
	signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	VkSubmitInfo2 submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submit.waitSemaphoreInfoCount = 1;
	submit.pWaitSemaphoreInfos = &waitInfo;
	submit.signalSemaphoreInfoCount = 1;
	submit.pSignalSemaphoreInfos = &signalInfo;
	submit.commandBufferInfoCount = 1;
	submit.pCommandBufferInfos = &cmdInfo;
	QueueSubmit2(m_queue, 1, &submit, frame.fence);

	VkPresentInfoKHR present = {};
	present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &m_images[m_imageIndex].renderFinished;
	present.swapchainCount = 1;
	present.pSwapchains = &m_swapchain;
	present.pImageIndices = &m_imageIndex;
	VkResult presented = vkQueuePresentKHR(m_queue, &present);
	m_recording = false;
	m_frame = (m_frame + 1) % kMaxFramesInFlight;
	if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR)
		RecreateSwapchain();
}

VkImageView CVulkanContext::CreateView(VkImage image, VkFormat format, VkImageAspectFlags aspect, uint32_t baseMip, uint32_t mipCount)
{
	VkImageViewCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.image = image;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.format = format;
	info.subresourceRange.aspectMask = aspect;
	info.subresourceRange.baseMipLevel = baseMip;
	info.subresourceRange.levelCount = mipCount;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	VkImageView view = VK_NULL_HANDLE;
	vkCreateImageView(m_device, &info, nullptr, &view);
	return view;
}

void CVulkanContext::DestroyView(VkImageView view)
{
	if (m_device && view)
		vkDestroyImageView(m_device, view, nullptr);
}

} // Vulkan3D
