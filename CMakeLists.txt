cmake_minimum_required(VERSION 3.5.0)

project(mari VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Vulkan REQUIRED)
#[[
function(add_shaders TARGET_NAME)
    set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files

    # Validate that source files have been passed
    list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files")
    endif()
    set(SHADER_COMMANDS)
    set(SHADER_PRODUCTS)
    foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)
        cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)
    
        # Build command
        list(APPEND SHADER_COMMAND COMMAND)
        list(APPEND SHADER_COMMAND Vulkan::glslc)
        list(APPEND SHADER_COMMAND "${SHADER_SOURCE}")
        list(APPEND SHADER_COMMAND "-o")
        list(APPEND SHADER_COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}.spv")
        # Add product
        list(APPEND SHADER_PRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_NAME}.spv")
    endforeach()
    add_custom_target(${TARGET_NAME} ALL
        ${SHADER_COMMANDS}
        COMMENT "Compiling ${TARGET_NAME}"
        SOURCES ${SHADER_SOURCE_FILES}
        BYPRODUCTS ${SHADER_PRODUCTS}
    )
endfunction()

add_shaders(shaders/simple.vert shaders/simple.frag)
#]]

#[[
add_custom_command(
    OUTPUT shader_compile
    COMMENT "Compiling shaders." 
    COMMAND "compile.bat"
)
#]]

include(CTest)
enable_testing()

add_executable(
    mari
    window.cpp
    mari.cpp
    device.cpp
    pipeline.cpp
    swapchain.cpp
    model.cpp
    game_object.cpp
    renderer.cpp
    simple_render_system.cpp
    camera.cpp
    keyboard_controller.cpp
)

#add_executable(mari)
#file(GLOB sources CONFIGURE_DEPENDS *.h *.c) 

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/glm")
target_link_libraries(mari glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/include/glfw-3.4")
target_link_libraries(mari glfw)

target_link_libraries(mari Vulkan::Vulkan)

target_sources(mari PRIVATE main.cpp)