include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required(VERSION 3.14.0)

set(NAME mari)
project(${NAME} VERSION 0.1.0)
 
# Set VULKAN_SDK_PATH in .env.cmake to target specific vulkan version
if (DEFINED VULKAN_SDK_PATH)
    set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include")
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib")
    set(Vulkan_FOUND "True")
else()
    find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
    message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Could not find Vulkan library!")
else()
    message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()
 
 
# Set GLFW_PATH in .env.cmake to target specific glfw
if (DEFINED GLFW_PATH)
    message(STATUS "Using GLFW path specified in .env")
    set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
    if (MSVC)
        set(GLFW_LIB "${GLFW_PATH}/lib-vc2022")
    endif()
else()
    find_package(glfw3 3.4 REQUIRED)
    set(GLFW_LIB glfw)
    message(STATUS "Found GLFW")
endif()
if (NOT GLFW_LIB)
    message(FATAL_ERROR "Could not find glfw library!")
else()
    message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

include_directories(include)
 
# Set FASTGLTF_PATH in .env.cmake
if (DEFINED FASTGLTF_PATH)
    message(STATUS "Using fastgltf path specified in .env")
    set(FASTGLTF_INCLUDE_DIRS "${FASTGLTF_PATH}/include")
    add_subdirectory("${FASTGLTF_PATH}") # TODO
endif()
if (NOT FASTGLTF_INCLUDE_DIRS)
    message(FATAL_ERROR "Could not find fastgltf!")
endif()

if (NOT DEFINED STB_PATH)
    message(FATAL_ERROR "Could not find stb!")
endif()

# Find OpenVDB
set(CMAKE_MODULE_PATH ${OPENVDB_INSTALL_PATH}/lib/cmake/OpenVDB/)
find_package(OpenVDB REQUIRED)

find_package(TBB REQUIRED)

# Get Dear ImGui source files plus what we need for GLFW and Vulkan 
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp ${IMGUI_PATH}/backends/imgui_impl_glfw.cpp ${IMGUI_PATH}/backends/imgui_impl_vulkan.cpp)

# Get MikkTSpace source files
file(GLOB MIKKTSPACE_SOURCES ${MIKKTSPACE_PATH}/*.c)

# Our project's source files
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES} ${MIKKTSPACE_SOURCES})

if(CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Generator is multi-config: ${CMAKE_GENERATOR}")
    message(STATUS "Available configs: ${CMAKE_CONFIGURATION_TYPES}")
else()
    message(STATUS "Generator is single-config: ${CMAKE_GENERATOR}")
    message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")
endif()

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_options(${PROJECT_NAME} PUBLIC
    $<$<CONFIG:Debug>: /Zi /DEBUG /W3>
    $<$<CONFIG:Release>: /O3>
)
add_compile_definitions("$<$<CONFIG:DEBUG>:MARI_DEBUG>")
#set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG") # use for release mode debugging

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/source
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_PATH}
    ${FASTGLTF_INCLUDE_DIRS}
    ${STB_PATH}
    ${IMGUI_PATH}
    ${IMGUI_PATH}/backends
    ${OpenVDB_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_LIBRARIES}
    ${GLFW_LIB}
    ${OpenVDB_LIBRARIES}
    ${Tbb_LIB_COMPONENTS}
)

target_link_libraries(${PROJECT_NAME} glfw3 vulkan-1 fastgltf OpenVDB::openvdb TBB::tbb)


############## Build SHADERS #######################
 
# Find all sources within shaders directory
# taken from VBlancos vulkan tutorial
# https://github.com/vblanco20-1/vulkan-guide/blob/all-chapters/CMakeLists.txt
find_program(GLSL_VALIDATOR glslangValidator HINTS 
    ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} 
    /usr/bin 
    /usr/local/bin 
    ${VULKAN_SDK_PATH}/Bin
    ${VULKAN_SDK_PATH}/Bin32
    $ENV{VULKAN_SDK}/Bin/
    $ENV{VULKAN_SDK}/Bin32/
)

# get all .vert and .frag files in shaders directory
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.rgen"
    "${PROJECT_SOURCE_DIR}/shaders/*.rchit"
    "${PROJECT_SOURCE_DIR}/shaders/*.rahit"
    "${PROJECT_SOURCE_DIR}/shaders/*.rmiss"
)

# TODO currently if a common.glsl file is changed everything is recompiled
# Ideally only those who include it should be recompiled
file(GLOB_RECURSE GLSL_COMMON_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.glsl"
)


foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/spv/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -g -V ${GLSL} -o ${SPIRV} --target-env vulkan1.4 # TODO -g -V only in debug
        DEPENDS ${GLSL} ${GLSL_COMMON_FILES})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(mari shaders)