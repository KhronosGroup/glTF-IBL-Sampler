cmake_minimum_required(VERSION 3.12.0 FATAL_ERROR)

macro(add_sources expression sources)
	file(GLOB source_files RELATIVE "${CMAKE_CURRENT_LIST_DIR}" "${expression}")
	set(${sources} "${${sources}};${source_files}")
endmacro()

macro(add_include_folder include_folder includes)
	set(${includes} "${${includes}};${CMAKE_CURRENT_LIST_DIR}/${include_folder}")
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

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#this project
project(glTFIBLSampler)

cmake_option(IBLSAMPLER_EXPORT_SHADERS "" OFF)

set(IBLSAMPLER_SHADERS_DIR "${CMAKE_CURRENT_LIST_DIR}/lib/shaders" CACHE STRING "")

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
add_sources("lib/source/*.cpp" "lib_sources")
add_sources("lib/include/*.h" "lib_sources")
set(lib_sources "${lib_sources};${Vulkan_INCLUDE_DIR}/../Source/layers/vk_format_utils.cpp")
set(lib_includes "${lib_includes};${Vulkan_INCLUDE_DIR}/../Source/layers")
add_include_folder("lib/include" "lib_includes")

set(STB_INCLUDE_PATH "${CMAKE_CURRENT_LIST_DIR}/thirdparty/stb/" CACHE STRING "")
set(lib_includes "${lib_includes};${STB_INCLUDE_PATH}")

#cli sources
add_sources("cli/source/*.cpp" "cli_sources")
add_include_folder("cli/include" "cli_includes")

#lib project
add_library(lib SHARED ${lib_sources} ${lib_includes})
target_include_directories(lib PUBLIC "${lib_includes}")

target_include_directories(lib PRIVATE "${Vulkan_INCLUDE_DIR}")
target_link_libraries(lib PRIVATE "${Vulkan_LIBRARIES}")

find_package(Glslang REQUIRED)
target_link_libraries(lib PRIVATE glslang SPIRV)
#source_group("include" FILES ${lib_includes})

# SLIMKTX2
add_subdirectory("thirdparty/slimktx2")
target_link_libraries(lib PRIVATE slimktx2)

#cli project
add_executable(cli "${cli_sources}")
target_include_directories(cli PRIVATE "${cli_includes}")
target_link_libraries(cli PUBLIC lib)

message(STATUS "")