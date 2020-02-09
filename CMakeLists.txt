cmake_minimum_required(VERSION 3.9)
project(mame_regtest)

option(USE_CONAN "Use conan for external dependencies" OFF)
option(USE_SANITIZERS "Use sanitizers" OFF)

# no system libraries on Windows so default to ON
if (WIN32)
    message(STATUS "Forcing usage of conan for external dependencies")
    set(USE_CONAN ON)
endif()

if (USE_CONAN)
    include(conan_deps.cmake)
endif()

find_package(Threads REQUIRED)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # CMake cannot find the static library for Visual Studio so provide it
    set(LIBXML2_LIBRARY ${CONAN_LIBXML2_ROOT}/lib/libxml2_a.lib)
endif()
find_package(LibXml2 REQUIRED)

find_package(ZLIB REQUIRED)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    file(DOWNLOAD https://github.com/tronkko/dirent/raw/1.23.2/include/dirent.h ${CMAKE_BINARY_DIR}/include/dirent.h
            EXPECTED_HASH SHA256=ce4cc82a7e1b1a2d9df607c893fc95b03847783d271bce86bb97ee5f2c42b82f
            TLS_VERIFY ON)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /WX)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Werror -Weverything)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Werror -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings -Wunreachable-code -Wpedantic -Wundef)
endif()

if (USE_SANITIZERS)
    # TODO: add more additional compiler-specific flags
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SANITITER_FLAGS -fsanitize=address -fsanitize=undefined)
        add_compile_options(${SANITITER_FLAGS} -fno-omit-frame-pointer)
        add_definitions(-DNO_ASSERT)
        add_link_options(${SANITITER_FLAGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # TODO: make this work
        # works only in 32-bit Release builds
        # add_compile_definitions(/fsanitize=address)
    endif()
endif()

if (MINWG32)
    add_compile_definitions(-DIN_LIBXML)
endif()

add_library(common-static STATIC common.c)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_include_directories(common-static PRIVATE SYSTEM ${CMAKE_BINARY_DIR}/include)
endif()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(common-static PRIVATE -Wno-sign-conversion -Wno-padded)
endif()
target_include_directories(common-static SYSTEM PRIVATE ${ZLIB_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})

add_library(config-static STATIC config.c)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(config-static PRIVATE -Wno-padded)
endif()
target_include_directories(config-static SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})

add_executable(mame_regtest mame_regtest.c)
target_include_directories(mame_regtest SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(mame_regtest PRIVATE -Wno-reserved-id-macro -Wno-padded)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(mame_regtest PRIVATE -Wno-format-truncation)
endif()
target_link_libraries(mame_regtest PRIVATE Threads::Threads common-static config-static ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(mame_regtest PRIVATE ws2_32)
endif()

add_executable(create_report create_report.c)
target_include_directories(create_report SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(create_report PRIVATE -Wno-cast-qual -Wno-disabled-macro-expansion -Wno-double-promotion -Wno-padded)
endif()
target_link_libraries(create_report PRIVATE Threads::Threads common-static config-static ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(create_report PRIVATE ws2_32)
endif()

add_executable(create_image_xml create_image_xml.c)
target_include_directories(create_image_xml SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(create_image_xml PRIVATE -Wno-padded)
endif()
target_link_libraries(create_image_xml PRIVATE Threads::Threads common-static ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(create_image_xml PRIVATE ws2_32)
endif()