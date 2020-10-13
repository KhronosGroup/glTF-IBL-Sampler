
set(external_project_name "Ktx")
set(external_project_target_name "ktx")
set(external_project_path "KTX-Software")
set(external_project_cmake_args "-DXCODE_CODE_SIGN_IDENTITY=")

if(MSVC)
    set(lib_name "${external_project_target_name}.dll")
elseif(APPLE)
    set(lib_name "lib${external_project_target_name}.dylib")
else()
    set(lib_name "lib${external_project_target_name}.so")
endif()

set(imported_target ${external_project_name}::${external_project_target_name})

# use a custom branch of basisu, which allows it being built as library
include(ExternalProject)
ExternalProject_Add(${external_project_name}
    PREFIX ${external_project_name}
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/${external_project_path}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> ${external_project_cmake_args}
    BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${lib_name}
    )
ExternalProject_Get_Property(${external_project_name} install_dir)

file(MAKE_DIRECTORY ${install_dir}/include)

add_library(${imported_target} SHARED IMPORTED GLOBAL)
set_target_properties(${imported_target} PROPERTIES
    IMPORTED_LOCATION ${install_dir}/lib/${lib_name}
    INTERFACE_INCLUDE_DIRECTORIES ${install_dir}/include)

add_dependencies(${imported_target} ${external_project_name})
