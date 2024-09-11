cmake_minimum_required(VERSION 3.5.0)

project(mari VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_executable(
    mari
    window.cpp
    renderer.cpp
)

#add_executable(mari)
#file(GLOB sources CONFIGURE_DEPENDS *.h *.c) 

include(CTest)
enable_testing()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/glm")
target_link_libraries(mari glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/glfw-3.4")
target_link_libraries(mari glfw)

find_package(Vulkan REQUIRED)
target_link_libraries(mari Vulkan::Vulkan)

target_sources(mari PRIVATE main.cpp)