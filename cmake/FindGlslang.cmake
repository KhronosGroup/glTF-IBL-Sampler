if(WIN32)
	find_path(Glslang_Parent_DIR
		NAMES "glslang/CMakeLists.txt"
		PATHS "$ENV{VULKAN_SDK}")
else()
	find_path(Glslang_Parent_DIR
		NAMES "glslang/CMakeLists.txt"
		PATHS "$ENV{VULKAN_SDK}/../source")
endif()

if (NOT Glslang_Parent_DIR STREQUAL "")
	set(SKIP_GLSLANG_INSTALL OFF CACHE INTERNAL "" FORCE)
	set(BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
	set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE INTERNAL "" FORCE)
	set(SPIRV_SKIP_EXECUTABLES ON CACHE INTERNAL "" FORCE)
	add_subdirectory("${Glslang_Parent_DIR}/glslang" "${CMAKE_CURRENT_BINARY_DIR}/glslang")
endif()