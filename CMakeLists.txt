cmake_minimum_required(VERSION 4.0)
project(hello_triangle_vulkan)

set(CMAKE_CXX_STANDARD 23)

add_executable(hello_triangle_vulkan main.cpp)

# Find Vulkan
find_package(Vulkan REQUIRED glslc)

# Require Vulkan version ≥ 1.3.256 (earliest version when the Vulkan module was available)
if( ${Vulkan_VERSION} VERSION_LESS "1.3.256" )
    message( FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
            "Found ${Vulkan_VERSION}."
    )
endif()

# If the shader compiler can't be found, exit.
if( NOT ${Vulkan_glslc} )
    message( FATAL_ERROR "Could not find slangc "
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

set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)

message( STATUS "Found Vulkan: ${Vulkan_INCLUDE_DIRS}" )
message( STATUS "Found slangcc: ${SLANGC_EXECUTABLE}" )

# Compile vulkan shaders
function (add_slang_shader_target TARGET)
    cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
    set (ENTRY_POINTS -entry vertMain -entry fragMain)
    add_custom_command (
            OUTPUT ${SHADERS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
    add_custom_command (
            OUTPUT  ${SHADERS_DIR}/slang.spv
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
    )
    add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

add_slang_shader_target(foo SOURCES ${SHADERS_DIR}/shader.slang)
add_dependencies(hello_triangle_vulkan foo)
