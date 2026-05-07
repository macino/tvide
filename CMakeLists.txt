cmake_minimum_required(VERSION 3.16)
project(tvide VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Build tvision as subdirectory
set(TV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/tvision)

# Vendored libvterm (for terminal emulator)
add_subdirectory(libvterm EXCLUDE_FROM_ALL)

file(GLOB TVTERM_CORE_SOURCES "src/tvterm-core/*.cc")

add_executable(tvide
    src/main.cpp
    src/app.cpp
    src/editor/editor.cpp
    src/editor/editwindow.cpp
    src/editor/linenums.cpp
    src/syntax/lexer.cpp
    src/syntax/php.cpp
    src/syntax/html.cpp
    src/syntax/css.cpp
    src/syntax/javascript.cpp
    src/syntax/typescript.cpp
    src/syntax/vue.cpp
    src/syntax/json.cpp
    src/syntax/markdown.cpp
    src/syntax/yaml.cpp
    src/syntax/sql.cpp
    src/syntax/xml.cpp
    src/syntax/clike.cpp
    src/syntax/python.cpp
    src/syntax/shell.cpp
    src/syntax/ruby.cpp
    src/syntax/dockerfile.cpp
    src/syntax/makefile.cpp
    src/syntax/ini.cpp
    src/syntax/plaintext.cpp
    src/dialogs/gotoline.cpp
    src/dialogs/findinfiles.cpp
    src/dialogs/findsymbol.cpp
    src/dialogs/openproject.cpp
    src/dialogs/about.cpp
    src/panels/filetree.cpp
    src/panels/terminal.cpp
    src/panels/messages.cpp
    src/panels/structure.cpp
    src/panels/winlist.cpp
    src/project/project.cpp
    ${TVTERM_CORE_SOURCES}
)

target_include_directories(tvide PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/tvision/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/tvision/include/tvision/compat/borland
    ${CMAKE_CURRENT_SOURCE_DIR}/libvterm/include
)

if(NOT WIN32)
    target_include_directories(tvide PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/tvision/include/tvision/compat/windows
    )
endif()

target_link_libraries(tvide PRIVATE tvision vterm-static)
if(NOT WIN32)
    target_link_libraries(tvide PRIVATE util)
endif()

target_compile_options(tvide PRIVATE
    -Wall -Wextra -Wno-deprecated -Wno-unknown-pragmas -Wno-missing-field-initializers
)

install(TARGETS tvide DESTINATION bin)
