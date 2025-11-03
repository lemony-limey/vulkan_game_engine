cmake_minimum_required(VERSION 4.0)
project(vulkan_for_real_this_time_maybe)

set(CMAKE_CXX_STANDARD 23)

add_executable(vulkan_for_real_this_time_maybe main.cpp)

find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glm::glm)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} glfw)
