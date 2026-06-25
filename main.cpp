#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

// Disabling constructors enables the use of initialiser lists.
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Vertex
{
    glm::vec2 position;
    glm::vec3 colour;

    // We've specified here that each space in the buffer will contain a Vertex object.
    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            // eVertex: move to the next data entry in the vertex buffer after each vertex
            // eInstance: move to the next data entry after each instance (we are not using
            //      instanced rendering here).
            .inputRate = vk::VertexInputRate::eVertex,
        };
    }

    // Attribute description describes the layout of each of the components within one vertex
    // element in the vertex buffer.
    // We've defined in here that location 0 contains the vertex's position, which we've
    // specified the location of in the vertex struct.
    //
    // We've then also specified that the second position (location 1) will contain the colour,
    // which we've specified the location of in the vertex struct also.
    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {{
            {
                .location = 0,
                .binding  = 0,
                .format   = vk::Format::eR32G32Sfloat,
                .offset   = offsetof(Vertex, position)
            },
            {
                .location = 1,
                .binding  = 0,
                .format   = vk::Format::eR32G32B32Sfloat,
                .offset   = offsetof(Vertex, colour)
            }
        }};
    }
};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};

const std::vector<Vertex> VERTICES = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
};

// You can use u16 or u32
const std::vector<uint16_t> INDICES = {
    0, 1, 2,
    2, 3, 0,
};

constexpr uint32_t    WIDTH  = 800;
constexpr uint32_t    HEIGHT = 600;
constexpr std::string SHADER_SPIRV_FILENAME = "../shaders/slang.spv";
constexpr int         MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow*                          m_window              = nullptr;
    vk::raii::Context                    m_context;
    vk::raii::Instance                   m_instance            = nullptr;
    vk::raii::DebugUtilsMessengerEXT     m_debugMessenger      = nullptr;
    vk::raii::SurfaceKHR                 m_surface             = nullptr;
    vk::raii::PhysicalDevice             m_physicalDevice      = nullptr;
    vk::raii::Device                     m_device              = nullptr;
    vk::raii::Queue                      m_queue               = nullptr;
    vk::raii::SwapchainKHR               m_swapChain           = nullptr;
    std::vector<vk::Image>               m_swapChainImages;
    std::vector<vk::raii::ImageView>     m_swapChainImageViews;
    vk::SurfaceFormatKHR                 m_swapChainSurfaceFormat;
    vk::Extent2D                         m_swapChainExtent;
    vk::raii::DescriptorSetLayout        m_descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout             m_pipelineLayout      = nullptr;
    vk::raii::Pipeline                   m_graphicsPipeline    = nullptr;
    uint32_t                             m_queueIndex          = ~0;
    vk::raii::CommandPool                m_commandPool         = nullptr;
    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    vk::raii::Buffer                     m_vertexBuffer        = nullptr;
    vk::raii::DeviceMemory               m_vertexBufferMemory  = nullptr;
    vk::raii::Buffer                     m_indexBuffer         = nullptr;
    vk::raii::DeviceMemory               m_indexBufferMemory   = nullptr;
    std::vector<vk::raii::Buffer>        m_uniformBuffers;
    std::vector<vk::raii::DeviceMemory>  m_uniformBuffersMemory;
    std::vector<void*>                   m_uniformBuffersMapped;
    vk::raii::DescriptorPool             m_descriptorPool      = nullptr;
    std::vector<vk::raii::DescriptorSet> m_descriptorSets;
    vk::raii::Image                      m_textureImage        = nullptr;
    vk::raii::DeviceMemory               m_textureImageMemory  = nullptr;
    vk::raii::ImageView                  m_textureImageView    = nullptr;
    vk::raii::Sampler                    m_textureSampler      = nullptr;

    // Synchronisation primitives.
    std::vector<vk::raii::Semaphore> m_presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     m_inFlightFences;
    uint32_t                         m_frameIndex = 0;

    bool m_framebufferResized = false;

    // Required device extensions.
    std::vector<const char *> m_requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
        "VK_KHR_portability_subset",
        vk::KHRSynchronization2ExtensionName,
        vk::KHRDynamicRenderingExtensionName,
    };

    void initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    }


    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = static_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }


    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }


    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }

        // Wait for logical device to finish all asynchronous operations
        // before exiting mainLoop and performing cleanup.
        m_device.waitIdle();
    }


    void cleanupSwapChain()
    {
        m_swapChainImageViews.clear();
        m_swapChain = nullptr;
    }


    void cleanup()
    {
        glfwDestroyWindow(m_window);

        glfwTerminate();
    }


    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        m_device.waitIdle();

        cleanupSwapChain();
        createSwapChain();
        createImageViews();
    }


    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties    = m_context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(
            requiredLayers,
            [&layerProperties](auto const& requiredLayer)
            {
                return std::ranges::none_of(
                    layerProperties,
                    [requiredLayer](auto const& layerProperty)
                    {
                        return strcmp(layerProperty.layerName, requiredLayer) == 0;
                    }
                );
            }
        );
        if (unsupportedLayerIt != requiredLayers.end())
        {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredInstanceExtensions();

        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties   = m_context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt =
                std::ranges::find_if(
                    requiredExtensions,
                    [&extensionProperties](auto const& requiredExtension)
                    {
                        return std::ranges::none_of(
                            extensionProperties,
                            [requiredExtension](auto const& extensionProperty)
                            {
                                return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                            }
                        );
                    }
                );
        if (unsupportedPropertyIt != requiredExtensions.end())
        {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }

        vk::InstanceCreateInfo createInfo{
            .flags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data(),
        };
        m_instance = vk::raii::Instance(m_context, createInfo);
    }


    std::vector<const char *> getRequiredInstanceExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector instance_extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            instance_extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        instance_extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);

        return instance_extensions;
    }


    void setupDebugMessenger()
    {
        if constexpr (!enableValidationLayers) { return; }

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        );
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlags,
            .pfnUserCallback = &debugCallback
        };
        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }


    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
        vk::DebugUtilsMessageTypeFlagsEXT             type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*

    )
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity ==
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage <<
                    std::endl;
        }

        return vk::False;
    }


    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        m_surface = vk::raii::SurfaceKHR(m_instance, _surface);
    }


    bool isDeviceSuitable(vk::raii::PhysicalDevice const& physical_device)
    {
        // Check if the physicalDevice supports the Vulkan 1.3 API version
        bool supportsVulkan1_3 = physical_device.getProperties().apiVersion >= VK_API_VERSION_1_3;

        // Check if any of the queue families support graphics operations
        auto queueFamilies    = physical_device.getQueueFamilyProperties();
        bool supportsGraphics = std::ranges::any_of(
            queueFamilies,
            [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); }
        );

        // Check if all required physicalDevice extensions are available
        auto availableDeviceExtensions     = physical_device.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
                std::ranges::all_of(
                    m_requiredDeviceExtension,
                    [&availableDeviceExtensions](auto const& requiredDeviceExtension)
                    {
                        return std::ranges::any_of(
                            availableDeviceExtensions,
                            [requiredDeviceExtension](auto const& availableDeviceExtension)
                            {
                                return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                            }
                        );
                    }
                );

        // Check if the physicalDevice supports the required features
        auto features =
                physical_device
                .template getFeatures2<vk::PhysicalDeviceFeatures2,
                    vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy
                                    && features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering
                                    && features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2
                                    && features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        // Return true if the physicalDevice meets all the criteria
        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }


    void pickPhysicalDevice()
    {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = m_instance.enumeratePhysicalDevices();
        // std::cout << "Number of physical devices: " << physicalDevices.size() << std::endl;

        if (physicalDevices.empty())
        {
            throw std::runtime_error("there are no Vulkan-compatible GPUs on the device");
        }

        // auto const devIter = std::ranges::find_if(
        //     physicalDevices,
        //     [&](auto const& physical_device) { return isDeviceSuitable(physical_device); }
        // );
        // if (devIter == physicalDevices.end())
        // {
        //     throw std::runtime_error("failed to find a suitable GPU!");
        // }
        // physicalDevice = *devIter;
        m_physicalDevice = physicalDevices.front();
    }


    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports both graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                m_physicalDevice.getSurfaceSupportKHR(qfpIndex, *m_surface))
            {
                // found a queue family that supports both graphics and present
                m_queueIndex = qfpIndex;
                break;
            }
        }
        if (m_queueIndex == ~0)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
            {.features = { .samplerAnisotropy = true } },                                                          // vk::PhysicalDeviceFeatures2
            {.shaderDrawParameters = true},                              // vk::PhysicalDeviceVulkan11Features
            {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        };

        // create a Device
        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = m_queueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(m_requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = m_requiredDeviceExtension.data()
        };

        m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
        m_queue  = vk::raii::Queue(m_device, m_queueIndex, 0);
    }


    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats)
    {
        assert(!availableFormats.empty());

        const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto &format)
            {
                return format.format == vk::Format::eB8G8R8Srgb
                    && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            }
        );

        return formatIt != availableFormats.end() ? *formatIt :availableFormats[0];
    }


    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
    {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(
            availablePresentModes,
            [](const vk::PresentModeKHR value)
            {
                return vk::PresentModeKHR::eMailbox == value;
            }) ?
            vk::PresentModeKHR::eMailbox :
            vk::PresentModeKHR::eFifo;
    }


    // Extent = resolution
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }


    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const & surfaceCapabilities)
    {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
        {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }


    void createSwapChain()
    {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface);
        m_swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = m_physicalDevice.getSurfaceFormatsKHR(*m_surface);
        m_swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

        std::vector<vk::PresentModeKHR> availablePresentModes = m_physicalDevice.getSurfacePresentModesKHR(*m_surface);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = *m_surface,
            .minImageCount = minImageCount,
            .imageFormat = m_swapChainSurfaceFormat.format,
            .imageColorSpace = m_swapChainSurfaceFormat.colorSpace,
            .imageExtent = m_swapChainExtent,
            .imageArrayLayers = 1,  // number of layers each image consists of
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = presentMode,
            .clipped = true
        };
        swapChainCreateInfo.oldSwapchain = nullptr;

        m_swapChain = vk::raii::SwapchainKHR(m_device, swapChainCreateInfo);
        m_swapChainImages = m_swapChain.getImages();
    }


    void createDescriptorSetLayout()
    {
        // Every binding needs to be described through a vk::DescriptorSetLayoutBinding struct.
        vk::DescriptorSetLayoutBinding uboLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,  // Number of values in the array. We only have 1 UBO.
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
        };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{
            .bindingCount = 1,
            .pBindings = &uboLayoutBinding,
        };
        m_descriptorSetLayout = vk::raii::DescriptorSetLayout(m_device, layoutInfo);
    }


    void createGraphicsPipeline()
    {
        // Below is state for the shader stages.

        vk::raii::ShaderModule shaderModule = createShaderModule(readFile(SHADER_SPIRV_FILENAME));
        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,  // stage we're currently operating in
            .module = shaderModule,
            .pName = "vertMain",
        };
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,  // stage we're currently operating in
            .module = shaderModule,
            .pName = "fragMain",
        };
        vk::PipelineShaderStageCreateInfo shaderStages[] = {
            vertShaderStageInfo,
            fragShaderStageInfo,
        };

        // Below is fixed-function state.

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions    = attributeDescriptions.data(),
        };

        // Lists of input vertices will represent triangles.
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
        };

        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data(),
        };

        vk::PipelineViewportStateCreateInfo viewportState{
            .viewportCount = 1,
            .scissorCount  = 1,
        };

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            // setting to true clamps fragments beyond the near and far planes,
            // which can be useful for shadow mapping.
            .depthClampEnable = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = vk::False,
            // line thickness in fragments (pixels).
            .lineWidth = 1.0f,
        };

        // Multisampling is a way to perform antialiasing, and is disabled for
        // now.
        vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable  = vk::False,
        };

        // Configure colour blending (currently disabled).
        vk::PipelineColorBlendAttachmentState colourBlendAttachment{
            .blendEnable    = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA
        };
        vk::PipelineColorBlendStateCreateInfo colourBlending{
            .logicOpEnable   = vk::False,
            .logicOp         = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments    = &colourBlendAttachment,
        };

        // Specify the pipeline layout after having initialised all of the fixed-function
        // state.

        // Create the pipeline layout.
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount         = 1,
            .pSetLayouts            = &*m_descriptorSetLayout,
            .pushConstantRangeCount = 0,
        };
        m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
            {
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = &vertexInputInfo,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pColorBlendState = &colourBlending,
                .pDynamicState = &dynamicState,
                .layout = m_pipelineLayout,
                // RenderPass is nullptr because we are using dynamic rendering.
                .renderPass = nullptr,
            },
            {
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &m_swapChainSurfaceFormat.format,
            }
        };

        // Create the graphics pipeline.

        m_graphicsPipeline = vk::raii::Pipeline(
            m_device,
            // We are not using a PipelineCache at the moment, but will later on.
            nullptr,
            pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
        );
    }


    static std::vector<char> readFile(const std::string& filename)
    {
        // std::cout << "pwd: " << std::filesystem::current_path() << "\n";
        // std::cout << "file: " << filename << std::endl;
        // ate = start reading at the end of the file; binary = interpret file
        // as a binary (i.e., no text transformations).
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        // Since we're starting at the end of the file, the starting position
        // is equal to the size of the file.
        std::vector<char> buffer(file.tellg());

        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

        file.close();

        return buffer;
    }


    // Create a shader module from the SPIR-V bytecode.
    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
    {
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        vk::raii::ShaderModule shaderModule{ m_device, createInfo };
        return shaderModule;
    }


    void createCommandPool()
    {
        const vk::CommandPoolCreateInfo poolInfo{
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = m_queueIndex,
        };

        m_commandPool = vk::raii::CommandPool(m_device, poolInfo);
    }


    void createCommandBuffers()
    {
        m_commandBuffers.clear();

        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = m_commandPool,
            // Primary means the buffer can be submitted to a queue for execution,
            // but cannot be called from other command buffers.
            // Secondary buffers cannot be submitted to a queue directly, but can
            // be called from primary command buffers.
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        };

        m_commandBuffers = vk::raii::CommandBuffers(m_device, allocInfo);
    }


    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto &commandBuffer = m_commandBuffers[m_frameIndex];
        commandBuffer.begin({});

        // Transition the image layout for rendering.
        transitionImageLayout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            // srcAccessMask (no need to wait for previous operations).
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        // Set up the colour attachment.
        vk::ClearValue clearColour = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            // imageView specifies which image view to render to.
            .imageView = m_swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = clearColour,
        };

        // Set up the rendering info.
        vk::RenderingInfo renderingInfo = {
            .renderArea = {
                .offset = {0, 0},
                .extent = m_swapChainExtent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &attachmentInfo,
        };

        // Begin rendering.
        commandBuffer.beginRendering(renderingInfo);

        // Rendering commands go here.
        // Bind to the pipeline.
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
        // We previously set the viewport and scissor state for this pipeline to be dynamic,
        // so we need to set them in the command buffer before issuing our draw command.
        commandBuffer.setViewport(
            0,
            vk::Viewport(
                0.0f,
                0.0f,
                static_cast<float>(m_swapChainExtent.width),
                static_cast<float>(m_swapChainExtent.height),
                0.0f,
                1.0f
            )
        );
        commandBuffer.setScissor(
            0,
            vk::Rect2D(
                vk::Offset2D(0, 0),
                m_swapChainExtent
            )
        );

        commandBuffer.bindVertexBuffers(0, *m_vertexBuffer, {0});
        // Could also use u32 if we needed more vertices, but we don't at the moment.
        commandBuffer.bindIndexBuffer(*m_indexBuffer, 0, vk::IndexType::eUint16);

        // Descriptor sets are not unique to graphics pipelines, so we need to specify whether we
        // want to bind them to the graphics or compute pipeline.
        m_commandBuffers[m_frameIndex].bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            m_pipelineLayout,
            0,
            *m_descriptorSets[m_frameIndex],
            nullptr  // Specifies an array of offsets used for dynamic descriptors.
        );

        commandBuffer.drawIndexed(
            static_cast<uint32_t>(INDICES.size()), // indexCount
            1, // instanceCount (1 is specified because instanced rendering is not used here)
            0, // firstIndex (offset into the index buffer for the lowest value of SV_VertexId)
            0, // vertexOffset (offset to add to the vertex index before indexing into the vertex buffer)
            0  // firstInstance (used as an offset for instanced rendering, defines the lowest SV_VertexId)
        );


        // End rendering.
        commandBuffer.endRendering();

        // Transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
        // for presentation.
        transitionImageLayout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe
        );

        // We've finished recording the command buffer.
        commandBuffer.end();
    }


    /**
     * Before we can start rendering to an image, we need to transition its
     * layout to one that is suitable for rendering.
     * In Vulkan, images can be in different layouts that are optimised for
     * different operations.
     * For example, an image can be in a layout that is optimal for presenting
     * to the screen, or in a layout that is optimal for being used as a
     * color attachment.
     */
    void transitionImageLayout(
        uint32_t                imageIndex,
        vk::ImageLayout         oldLayout,
        vk::ImageLayout         newLayout,
        vk::AccessFlags2        srcAccessMask,
        vk::AccessFlags2        destAccessMask,
        vk::PipelineStageFlags2 srcStageMask,
        vk::PipelineStageFlags2 destStageMask
    )
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask = srcStageMask,
            .srcAccessMask = srcAccessMask,
            .dstStageMask = destStageMask,
            .dstAccessMask = destAccessMask,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_swapChainImages[imageIndex],
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        vk::DependencyInfo dependencyInfo = {
            .dependencyFlags = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrier,
        };

        m_commandBuffers[m_frameIndex].pipelineBarrier2(dependencyInfo);
    }


    void createSyncObjects()
    {
        assert(m_presentCompleteSemaphores.empty()
            && m_renderFinishedSemaphores.empty()
            && m_inFlightFences.empty()
        );

        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());

        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_presentCompleteSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
            m_inFlightFences.emplace_back(
                m_device,
                vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled }
            );
        }
    }


    // Note that inFlightFences, presentCompleteSemaphores and m_commandBuffers are indexed by frameIndex,
    // whereas renderFinishedSemaphores is indexed by image index.
    // TODO: Update this comment when variables are renamed.
    void drawFrame()
    {
        // Fences allow the ability to reorder load and store operations.
        auto fenceResult = m_device.waitForFences(*m_inFlightFences[m_frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }

        auto [result, imageIndex] = m_swapChain.acquireNextImage(UINT64_MAX, *m_presentCompleteSemaphores[m_frameIndex], nullptr);

        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defines, eErrorOutOfDateKHR
        // can be checked as a result.
        if (result == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
        {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        updateUniformBuffer(m_frameIndex);

        // Only reset the fence if we are submitting work.
        m_device.resetFences(*m_inFlightFences[m_frameIndex]);

        m_commandBuffers[m_frameIndex].reset();
        recordCommandBuffer(imageIndex);

        m_queue.waitIdle();
        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*m_presentCompleteSemaphores[m_frameIndex],
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*m_commandBuffers[m_frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*m_renderFinishedSemaphores[imageIndex],
        };

        m_queue.submit(submitInfo, *m_inFlightFences[m_frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*m_renderFinishedSemaphores[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &*m_swapChain,
            .pImageIndices = &imageIndex
        };

        result = m_queue.presentKHR(presentInfoKHR);

        // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
        // here and does not need to be caught by an exception.
        if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapChain();
        }
        else
        {
            // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
            assert(result == vk::Result::eSuccess);
        }

        // switch (result)
        // {
        //     case vk::Result::eSuccess:
        //         // std::cout << "Success" << std::endl;
        //         break;
        //     case vk::Result::eSuboptimalKHR:
        //         std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
        //         break;
        //     default:
        //         std::cout << "Unexpected result" << std::endl;
        //         break;        // an unexpected result is returned!
        // }

        m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }


    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = rotate(
            glm::mat4(1.0f),
            time * glm::radians(90.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.view = lookAt(
            glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        );
        ubo.projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height),
            0.1f,
            10.0f
        );
        // Need to compensate for the inverted Y coordinates from OpenGL, since GLM was designed for OpenGL.
        ubo.projection[1][1] *= -1;

        // Push constants are a more efficient way to frequently pass changing values to a shader.
        memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }


    void createTextureImage()
    {
        int textureWidth, textureHeight, textureChannels;
        stbi_uc* pixels = stbi_load("../astley.jpg", &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = textureWidth * textureHeight * 4;

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            imageSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        void* data = stagingBufferMemory.mapMemory(0, imageSize);
        memcpy(data, pixels, imageSize);
        stagingBufferMemory.unmapMemory();

        // Cleanup
        stbi_image_free(pixels);

        std::tie(m_textureImage, m_textureImageMemory) = createImage(
            textureWidth,
            textureHeight,
            vk::Format::eR8G8B8A8Srgb,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
        transitionImageLayout(
            commandBuffer,
            m_textureImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eTransferDstOptimal
        );
        copyBufferToImage(
            commandBuffer,
            stagingBuffer,
            m_textureImage,
            static_cast<uint32_t>(textureWidth),
            static_cast<uint32_t>(textureHeight)
        );
        transitionImageLayout(
            commandBuffer,
            m_textureImage,
            vk::ImageLayout::eTransferDstOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal
        );
        endSingleTimeCommand(std::move(commandBuffer));
    }


    std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
        uint32_t                width,
        uint32_t                height,
        vk::Format              format,
        vk::ImageTiling         tiling,
        vk::ImageUsageFlags     usage,
        vk::MemoryPropertyFlags properties
    )
    {
        vk::ImageCreateInfo imageInfo{
            .imageType   = vk::ImageType::e2D,  // What coordinate system (e.g., 1D (data), 2D (texture), 3D (voxel volume))
            .format      = format,
            .extent      = {width, height, .depth = 1},  // How many texels on each axis.
            .mipLevels   = 1,
            .arrayLayers = 1,
            .samples     = vk::SampleCountFlagBits::e1,
            .tiling      = tiling,  // How texels are arranged in memory.
            .usage       = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        };

        vk::raii::Image image = vk::raii::Image(m_device, imageInfo);

        vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = findMemoryType(
                memRequirements.memoryTypeBits,
                properties
            )
        };

        vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(m_device, allocInfo);
        image.bindMemory(imageMemory, 0);

        return {
            std::move(image),
            std::move(imageMemory)
        };
    }


    vk::raii::ImageView createImageView(vk::Image const & image, vk::Format format)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo{
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };
        return vk::raii::ImageView(m_device, imageViewCreateInfo);
    }


    void createImageViews()
    {
        assert(m_swapChainImageViews.empty());

        m_swapChainImageViews.reserve(m_swapChainImages.size());
        for (auto & image : m_swapChainImages)
        {
            m_swapChainImageViews.emplace_back(createImageView(image, m_swapChainSurfaceFormat.format));
        }
    }


    void createTextureImageView()
    {
        m_textureImageView = createImageView(*m_textureImage, vk::Format::eR8G8B8A8Srgb);
    }


    void createTextureSampler()
    {
        vk::PhysicalDeviceProperties properties = m_physicalDevice.getProperties();
        vk::SamplerCreateInfo samplerInfo{
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = vk::True,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = vk::False,
        };

        m_textureSampler = vk::raii::Sampler(m_device, samplerInfo);
    }


    void createVertexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(VERTICES[0]) * VERTICES.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        // Keep in mind that while the driver will know we have written this data to the buffer,
        // the GPU will not know until we call vk::raii::Queue::submit().
        void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, VERTICES.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(m_vertexBuffer, m_vertexBufferMemory) = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
    }


    void createIndexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(INDICES[0]) * INDICES.size();

        auto [stagingBuffer, stagingBufferMemory] = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
        );

        // Keep in mind that while the driver will know we have written this data to the buffer,
        // the GPU will not know until we call vk::raii::Queue::submit().
        void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, INDICES.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(m_indexBuffer, m_indexBufferMemory) = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);
    }


    void createUniformBuffers()
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
            auto [buffer, bufferMemory] = createBuffer(
                bufferSize,
                vk::BufferUsageFlagBits::eUniformBuffer,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            );
            m_uniformBuffers.emplace_back(std::move(buffer));
            m_uniformBuffersMemory.emplace_back(std::move(bufferMemory));
            // We retain the pointer from the mapping as it will stay mapped for the application's lifetime.
            // This is known as "persistent mapping" and avoids the overhead of mapping and freeing every time.
            m_uniformBuffersMapped.emplace_back(m_uniformBuffersMemory.back().mapMemory(0, bufferSize));
        }
    }


    /**
     * Descriptor sets can't be created directly, they must be allocated from a pool, just like
     * command buffers.
     */
    void createDescriptorPool()
    {
        // Describe which descriptor types our descriptor sets are going to contains and how many
        // of them there will be.
        vk::DescriptorPoolSize poolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        };
        // One descriptor will be allocated for every frame. This pool size structure is referenced
        // by the main vk::DescriptorPoolCreateInfo struct.
        vk::DescriptorPoolCreateInfo poolInfo{
            // This flag is necessary so that the RAII handle is allowed to destroy the
            // underlying VkDescriptorSet when dropped.
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = MAX_FRAMES_IN_FLIGHT,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize,
        };

        m_descriptorPool = vk::raii::DescriptorPool(m_device, poolInfo);
    }


    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *m_descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = m_descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
        };

        // Allocate the descriptor sets.
        m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{
                .buffer = m_uniformBuffers[i],
                .offset = 0,
                .range = sizeof(UniformBufferObject),
            };
            vk::WriteDescriptorSet descriptorWrite{
                .dstSet          = m_descriptorSets[i],
                .dstBinding      = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType  = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo     = &bufferInfo,
            };

            m_device.updateDescriptorSets(descriptorWrite, {});
        }
    }


    /**
     *
     * @param typeFilter Used to specify the bit field of memory types that are suitable.
     *      This means that we can find the index of a suitable memory type by simply
     *      iterating over them and checking if the corresponding bit is set to 1.
     * @param properties
     * @return
     */
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memoryProperties = m_physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }


    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
    {
        vk::BufferCreateInfo bufferInfo{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        };
        vk::raii::Buffer buffer = vk::raii::Buffer(m_device, bufferInfo);
        vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, properties),
        };
        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(m_device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
        // vk::raii handles can't be copied (because they're dropped when out of scope) so they have
        // to be moved out of the local scope.
        return {std::move(buffer), std::move(bufferMemory)};
    }


    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) const
    {
        vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
        // Copy everything from the source buffer from position 0 onwards to the end, to the destination buffer.
        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});

        endSingleTimeCommand(std::move(commandCopyBuffer));
    }


    vk::raii::CommandBuffer beginSingleTimeCommands() const
    {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(m_device, allocInfo).front());

        // Good practice to indicate that we're only going to use the command buffer once.
        vk::CommandBufferBeginInfo beginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };
        commandBuffer.begin(beginInfo);

        return std::move(commandBuffer);
    }


    void endSingleTimeCommand(vk::raii::CommandBuffer && commandBuffer) const
    {
        commandBuffer.end();

        const vk::SubmitInfo submitInfo{
            .commandBufferCount = 1,
            .pCommandBuffers = &*commandBuffer,
        };
        m_queue.submit(submitInfo, nullptr);
        m_queue.waitIdle();
    }


    void transitionImageLayout(
        vk::raii::CommandBuffer & commandBuffer,
        const vk::raii::Image & image,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout
    )
    {
        vk::ImageMemoryBarrier barrier{
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage      = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
    }


    void copyBufferToImage(
        vk::raii::CommandBuffer & commandBuffer,
        const vk::raii::Buffer  & buffer,
        vk::raii::Image         & image,
        uint32_t                  width,
        uint32_t                  height
    )
    {
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = { 0, 0, 0 },
            .imageExtent = { width, height, 1 },
        };
        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
    }
};


int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
