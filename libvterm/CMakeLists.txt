# CMake script for libvterm originally created by Rui Abreu Ferreira
# (https://github.com/equalsraf/libvterm-win).

cmake_minimum_required(VERSION 3.5)

find_package(Perl REQUIRED)

set(VTERM_DIR ${CMAKE_CURRENT_LIST_DIR})

# Generate includes from tables
file(GLOB TBL_FILES ${VTERM_DIR}/src/encoding/*.tbl)
set(TBL_FILES_HEADERS)
foreach(file ${TBL_FILES})
    get_filename_component(basename ${file} NAME_WE)
    set(tname encoding/${basename}.inc)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${tname}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/encoding/
        COMMAND ${PERL_EXECUTABLE} -CSD ${VTERM_DIR}/tbl2inc_c.pl ${file} > ${CMAKE_CURRENT_BINARY_DIR}/${tname}
        DEPENDS ${file}
        COMMENT "Generating ${tname}"
    )
    list(APPEND TBL_FILES_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${tname})
endforeach()

file(GLOB VTERM_SOURCES ${VTERM_DIR}/src/*.c)
add_library(vterm-static STATIC ${VTERM_SOURCES} ${TBL_FILES_HEADERS})
target_include_directories(vterm-static PUBLIC
    "$<BUILD_INTERFACE:${VTERM_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_include_directories(vterm-static PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(MSVC)
    target_compile_definitions(vterm-static PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
else()
    set_property(TARGET vterm-static PROPERTY C_STANDARD 99)
    set_property(TARGET vterm-static PROPERTY C_STANDARD_REQUIRED ON)
endif()
