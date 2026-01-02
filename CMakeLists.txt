cmake_minimum_required(VERSION 4.0)
project(hello_triangle_vulkan)

set(CMAKE_CXX_STANDARD 23)

add_executable(hello_triangle_vulkan main.cpp)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Require Vulkan version ≥ 1.3.256 (earliest version when the Vulkan module was available)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
    message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
            "Found ${Vulkan_VERSION}."
    )
endif()

if( ${Vulkan_VERSION} VERSION_GREATER_EQUAL "1.3.256" )
    message( STATUS "Found Vulkan version: ${Vulkan_VERSION}.")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw)
