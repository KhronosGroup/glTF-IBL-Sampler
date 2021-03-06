cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)

macro(add_sources expression sources)
    file(GLOB source_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${expression}")
    set(${sources} "${${sources}};${source_files}")
endmacro()

macro(add_include_folder include_folder includes)
    set(${includes} "${${includes}};${CMAKE_CURRENT_SOURCE_DIR}/${include_folder}")
endmacro()

function(cmake_option option description default)
    option(${option} description ${default})
    if(${option})
        message(STATUS "'${option}' is TRUE")
    else()
        message(STATUS "'${option}' is FALSE")
    endif()
endfunction()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS TRUE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Linux")
    # use, i.e. don't skip the full RPATH for the build tree
    set(CMAKE_SKIP_BUILD_RPATH FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    # the RPATH to be used when installing
    set(CMAKE_INSTALL_RPATH "\$ORIGIN")
    #set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX_ABSOLUTE_PATH}/usr/lib")

    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

    set(CMAKE_SKIP_RPATH FALSE)
endif()


#this project
project(glTFIBLSampler)

cmake_option(IBLSAMPLER_EXPORT_SHADERS "" OFF)

set(IBLSAMPLER_SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/shaders" CACHE STRING "")

if (IBLSAMPLER_EXPORT_SHADERS)
    set(IBLSAMPLER_SHADERS_DIR "shaders" CACHE INTERNAL "" FORCE)
endif()

add_definitions(-DIBLSAMPLER_SHADERS_DIR="${IBLSAMPLER_SHADERS_DIR}")

if (IBLSAMPLER_EXPORT_SHADERS)
    if (WIN32)
        file(COPY "lib/shaders" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/")
        file(COPY "lib/shaders" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/")
    else()
        file(COPY "lib/shaders" DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/")
    endif()
endif()

#dependencies
find_package(Vulkan REQUIRED)

#lib sources
add_sources("lib/source/*.cpp" lib_sources)
add_sources("lib/source/*.h" lib_sources)
add_sources("${CMAKE_CURRENT_SOURCE_DIR}/lib/include/*.h" lib_headers)
add_include_folder("lib/include" lib_include_dirs)

# STB
set(STB_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb/" CACHE STRING "")
set(lib_include_dirs "${lib_include_dirs};${STB_INCLUDE_PATH}")

# glslang
option(BUILD_EXTERNAL "Build external dependencies in /External" OFF)
option(SKIP_GLSLANG_INSTALL "Skip installation" ON)
set(ENABLE_GLSLANG_INSTALL OFF)
option(ENABLE_SPVREMAPPER "Enables building of SPVRemapper" OFF)
option(ENABLE_GLSLANG_BINARIES "Builds glslangValidator and spirv-remap" OFF)
option(ENABLE_GLSLANG_WEB "Reduces glslang to minimum needed for web use" OFF)
option(ENABLE_GLSLANG_WEB_DEVEL "For ENABLE_GLSLANG_WEB builds, enables compilation error messages" OFF)
option(ENABLE_EMSCRIPTEN_SINGLE_FILE "If using Emscripten, enables SINGLE_FILE build" OFF)
option(ENABLE_EMSCRIPTEN_ENVIRONMENT_NODE "If using Emscripten, builds to run on Node instead of Web" OFF)
option(ENABLE_HLSL "Enables HLSL input support" OFF)

if(NOT TARGET glslang) 
    # we need to guard these for the case that a higher level project already added the target
    add_subdirectory(thirdparty/glslang)
endif()

#lib project
add_library(GltfIblSampler SHARED ${lib_sources} ${lib_headers})
target_include_directories(GltfIblSampler PUBLIC "${lib_include_dirs}")
# specify the public headers (will be copied to `include` in install step)
set_target_properties(GltfIblSampler PROPERTIES PUBLIC_HEADER "${lib_headers}")

# glslang
target_link_libraries(GltfIblSampler PRIVATE glslang SPIRV)

# Vulkan
target_link_libraries(GltfIblSampler PRIVATE Vulkan::Vulkan)

# libktx
include(thirdparty/KTX-Software.cmake)
target_link_libraries(GltfIblSampler PRIVATE Ktx::ktx)

#cli project
add_sources("cli/source/*.cpp" "cli_sources")
add_executable(cli "${cli_sources}")
target_link_libraries(cli PUBLIC GltfIblSampler)

message(STATUS "")
install(TARGETS cli GltfIblSampler)

#copy ktx.dll to right folder in multi-build configs
add_custom_command(TARGET GltfIblSampler
  COMMAND ${CMAKE_COMMAND}
        -DMSVC_BUILD_CONFIG_DIR=${CMAKE_CFG_INTDIR}
        -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_BINARY_DIR}
        -P "${PROJECT_SOURCE_DIR}/cmake/copy_dlls.cmake"
  VERBATIM
) 
