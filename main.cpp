#include <vulkan/vulkan_raii.hpp>

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>

constexpr uint32_t WIDTH  = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
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
    // Members

    vk::raii::Context                m_context;
    vk::raii::Device                 m_device         = nullptr;
    vk::raii::DebugUtilsMessengerEXT m_debugMessenger = nullptr;
    vk::raii::Instance               m_instance       = nullptr;
    vk::raii::PhysicalDevice         m_physicalDevice = nullptr;
    vk::raii::Queue                  m_graphicsQueue  = nullptr;
    vk::raii::SurfaceKHR             m_surface        = nullptr;
    GLFWwindow*                      m_window         = nullptr;
    std::vector<const char*> m_deviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName,
    };


    // Methods

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
        pickPhysicalDevice();
        createLogicalDevice();
    }


    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
        }
    }


    void cleanup()
    {
        glfwDestroyWindow(m_window);

        // Setting the debug messenger to nullptr puts it out of scope, and therefore
        // it is deallocated.
        m_debugMessenger = nullptr;

        glfwTerminate();
    }


    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{
            "Hello Triangle",
            VK_MAKE_VERSION(1, 0, 0),
            "No Engine",
            VK_MAKE_VERSION(1, 0, 0),
            vk::ApiVersion14
        };

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = m_context.enumerateInstanceLayerProperties();
        for (auto const& requiredLayer : requiredLayers)
        {
            if (std::ranges::none_of(
                layerProperties,
                [requiredLayer](auto const& layerProperty) -> bool
                {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                }
            ))
            {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }

        // Get the required instance extensions from GLFW.
        auto requiredExtensions = getRequiredExtensions();

        // Check if the required GLFW extensions are supported by the Vulkan implementation.
        auto extensionProperties = m_context.enumerateInstanceExtensionProperties();
        for (auto const& requiredExtension : requiredExtensions)
        {
            if (std::ranges::none_of(
                extensionProperties,
                [requiredExtension](auto const& extensionProperty) -> bool
                {
                    return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                }
            ))
            {
                throw std::runtime_error("Required GLFW extension not supported: " + std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo(
            {vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR},
            &appInfo,
            requiredLayers,
            requiredExtensions
        );

        m_instance = vk::raii::Instance(m_context, createInfo);
    }


    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);

        return extensions;
    }


    void setupDebugMessenger()
    {
        if (!enableValidationLayers) { return; }

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        );
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
            | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{};
        debugUtilsMessengerCreateInfoEXT.messageSeverity = severityFlags;
        debugUtilsMessengerCreateInfoEXT.messageType     = messageTypeFlags;
        debugUtilsMessengerCreateInfoEXT.pfnUserCallback = &debugCallback;

        m_debugMessenger = m_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }


    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
        vk::DebugUtilsMessageTypeFlagsEXT             type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*
    )
    {
        if (severity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage <<
                    std::endl;
        }

        return vk::False;
    }


    void pickPhysicalDevice()
    {
        // Evaluate each device and check if they are suitable for the operations we want to perform.
        // This is necessary due to differences between graphics cards from different
        // manufacturers.
        std::vector<vk::raii::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
        const auto devIter = std::ranges::find_if(
            devices,
            [&](auto const& device)
            {
                auto queueFamilies = device.getQueueFamilyProperties();
                bool isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
                const auto qfpIter = std::ranges::find_if(
                    queueFamilies,
                    [](vk::QueueFamilyProperties const& qfp) -> bool
                    {
                        return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                    }
                );
                isSuitable = isSuitable && (qfpIter != queueFamilies.end());

                auto extensions = device.enumerateDeviceExtensionProperties();
                bool found = true;
                for (auto const& extension : m_deviceExtensions)
                {
                    auto extensionIter = std::ranges::find_if(
                        extensions,
                        [extension](auto const& ext) -> bool
                        {
                            return strcmp(ext.extensionName, extension) == 0;
                        }
                    );
                    found = found && (extensionIter != extensions.end());
                }
                isSuitable = isSuitable && found;

                if (isSuitable)
                {
                    m_physicalDevice = device;
                }
                return isSuitable;
            }
        );

        // If there are no devices to support, there is no point in going further.
        if (devIter == devices.end())
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    // In Vulkan, operations must be submitted to a queue.
    //
    // Different types of queues originate from different queue families.
    //
    // Each family of queues allows only a subset of commands.
    // For example, there could be a queue family that only allows processing of compute
    // commmands or one that only allows memory transfer related commands.
    uint32_t findQueueFamilies(const vk::raii::PhysicalDevice& physicalDevice)
    {
        // Find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // Get the first index into queueFamilyProperties which supports graphics
        auto graphicsQueueFamilyProperty = std::find_if(
            queueFamilyProperties.begin(),
            queueFamilyProperties.end(),
            [](vk::QueueFamilyProperties const& qfp)
            {
                return qfp.queueFlags & vk::QueueFlagBits::eGraphics;
            }
        );

        return static_cast<uint32_t>(
            std::distance(
                queueFamilyProperties.begin(),
                graphicsQueueFamilyProperty
            )
        );
    }

    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physicalDevice)
    {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        // The device is suitable if it is a discrete GPU with a geometry shader.
        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader)
        {
            return true;
        }

        return false;
    }

    void createLogicalDevice()
    {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_physicalDevice.getQueueFamilyProperties();
        uint32_t graphicsIndex = findQueueFamilies(m_physicalDevice);
        float queuePriority = 0.5f;

        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{};
        deviceQueueCreateInfo.queueFamilyIndex = graphicsIndex;
        deviceQueueCreateInfo.queueCount = 1;
        deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

        vk::PhysicalDeviceFeatures deviceFeatures;

        // Empty for now.
        vk::PhysicalDeviceFeatures2 deviceForChain1{};
        // Enable dynamic rendering from Vulkan 1.3.
        vk::PhysicalDeviceVulkan13Features deviceForChain2{};
        deviceForChain2.dynamicRendering = true;
        // Enable extended dynamic state from the extension.
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT deviceForChain3{};
        deviceForChain3.extendedDynamicState = true;

        // The structure chain automatically sets up the pNext pointers between
        // the structures.
        vk::StructureChain<
            vk::PhysicalDeviceFeatures2,
            vk::PhysicalDeviceVulkan13Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        > featureChain(
            deviceForChain1,
            deviceForChain2,
            deviceForChain3
        );

        vk::DeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>();
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

        m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);
        m_graphicsQueue = vk::raii::Queue(m_device, graphicsIndex, 0);
    }

};


int main()
{
    HelloTriangleApplication app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
