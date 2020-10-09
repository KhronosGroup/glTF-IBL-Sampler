
if(MSVC)
    set(ktx_lib_name "ktx.dll")
elseif(APPLE)
    set(ktx_lib_name "libktx.dylib")
else()
    set(ktx_lib_name "libktx.so")
endif()

# use a custom branch of basisu, which allows it being built as library
include(ExternalProject)
ExternalProject_Add(ktx-software-project
    PREFIX KTX-Software
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/thirdparty/KTX-Software
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DXCODE_CODE_SIGN_IDENTITY=
    BUILD_BYPRODUCTS <INSTALL_DIR>/lib/${ktx_lib_name}
    )
ExternalProject_Get_Property(ktx-software-project install_dir)
set(ktx-software_install_dir ${install_dir})

add_library(ktx-software-library STATIC IMPORTED GLOBAL)
set_property(TARGET ktx-software-library  PROPERTY IMPORTED_LOCATION ${ktx-software_install_dir}/lib/${ktx_lib_name})
add_dependencies(ktx-software-library ktx-software-project)
