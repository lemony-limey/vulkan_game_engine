#include <algorithm>
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

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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
    GLFWwindow*                          m_window             = nullptr;
    vk::raii::Context                    m_context;
    vk::raii::Instance                   m_instance           = nullptr;
    vk::raii::DebugUtilsMessengerEXT     m_debugMessenger     = nullptr;
    vk::raii::SurfaceKHR                 m_surface            = nullptr;
    vk::raii::PhysicalDevice             m_physicalDevice     = nullptr;
    vk::raii::Device                     m_device             = nullptr;
    vk::raii::Queue                      m_queue              = nullptr;
    vk::raii::SwapchainKHR               m_swapChain          = nullptr;
    std::vector<vk::Image>               m_swapChainImages;
    std::vector<vk::raii::ImageView>     m_swapChainImageViews;
    vk::SurfaceFormatKHR                 m_swapChainSurfaceFormat;
    vk::Extent2D                         m_swapChainExtent;
    vk::raii::PipelineLayout             m_pipelineLayout     = nullptr;
    vk::raii::Pipeline                   m_graphicsPipeline   = nullptr;
    uint32_t                             m_queueIndex         = ~0;
    vk::raii::CommandPool                m_commandPool        = nullptr;
    std::vector<vk::raii::CommandBuffer> m_commandBuffers;
    vk::raii::Buffer                     m_vertexBuffer       = nullptr;
    vk::raii::DeviceMemory               m_vertexBufferMemory = nullptr;
    vk::raii::Buffer                     m_indexBuffer        = nullptr;
    vk::raii::DeviceMemory               m_indexBufferMemory  = nullptr;

    // Synchronisation primitives.
    vk::raii::Semaphore m_presentCompleteSemaphore = nullptr;
    vk::raii::Semaphore m_renderFinishedSemaphore  = nullptr;
    vk::raii::Fence     m_drawFence                = nullptr;
    uint32_t            m_frameIndex                 = 0;

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
        createGraphicsPipeline();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
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


    void cleanup()
    {
        glfwDestroyWindow(m_window);

        glfwTerminate();
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
                .template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features,
                    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().
                                                 extendedDynamicState;

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
            {},                                                          // vk::PhysicalDeviceFeatures2
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


    void createImageViews()
    {
        assert(m_swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D,
            .format = m_swapChainSurfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };

        for (auto &image : m_swapChainImages)
        {
            imageViewCreateInfo.image = image;
            m_swapChainImageViews.emplace_back(m_device, imageViewCreateInfo);
        }
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
            .frontFace = vk::FrontFace::eClockwise,
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
            .setLayoutCount         = 0,
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
        m_presentCompleteSemaphore = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo());
        m_renderFinishedSemaphore  = vk::raii::Semaphore(m_device, vk::SemaphoreCreateInfo());
        m_drawFence = vk::raii::Fence(
            m_device,
            { .flags = vk::FenceCreateFlagBits::eSignaled }
        );
    }


    // Note that inFlightFences, presentCompleteSemaphores and m_commandBuffers are indexed by frameIndex,
    // whereas renderFinishedSemaphores is indexed by image index.
    // TODO: Update this comment when variables are renamed.
    void drawFrame()
    {
        // Fences allow the ability to reorder load and store operations.
        auto fenceResult = m_device.waitForFences(*m_drawFence, vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }
        m_device.resetFences(*m_drawFence);

        auto [result, imageIndex] = m_swapChain.acquireNextImage(UINT64_MAX, *m_presentCompleteSemaphore, nullptr);

        m_commandBuffers[m_frameIndex].reset();
        recordCommandBuffer(imageIndex);

        m_queue.waitIdle();
        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*m_presentCompleteSemaphore,
            .pWaitDstStageMask = &waitDestinationStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &*m_commandBuffers[m_frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &*m_renderFinishedSemaphore,
        };

        m_queue.submit(submitInfo, *m_drawFence);

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*m_renderFinishedSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &*m_swapChain,
            .pImageIndices = &imageIndex
        };

        result = m_queue.presentKHR(presentInfoKHR);
        switch (result)
        {
            case vk::Result::eSuccess:
                // std::cout << "Success" << std::endl;
                break;
            case vk::Result::eSuboptimalKHR:
                std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
                break;
            default:
                std::cout << "Unexpected result" << std::endl;
                break;        // an unexpected result is returned!
        }

        m_frameIndex = (m_frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
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
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        vk::raii::CommandBuffer commandCopyBuffer = std::move(m_device.allocateCommandBuffers(allocInfo).front());
        // Good practice to indicate that we're only going to use the command buffer once.
        commandCopyBuffer.begin({
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        });
        // Copy everything from the source buffer from position 0 onwards to the end, to the destination buffer.
        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));
        commandCopyBuffer.end();

        m_queue.submit(
            vk::SubmitInfo{
                .commandBufferCount = 1,
                .pCommandBuffers = &*commandCopyBuffer,
            },
            nullptr
        );
        m_queue.waitIdle();
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
