cmake_minimum_required(VERSION 3.9)
project(mame_regtest)

option(USE_CONAN "Use conan for external dependencies" OFF)
option(USE_SANITIZERS "Use sanitizers" OFF)
option(USE_VS_ANALYZE "Use Visual Studio Code Analysis" OFF)
option(USE_GCC_ANALYZER "Use GCC Static Analysis" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# no system libraries on Windows so default to ON
if (WIN32)
    message(STATUS "Forcing usage of conan for external dependencies")
    set(USE_CONAN ON)
endif()

if (USE_CONAN)
    message(STATUS "Using conan for external dependencies")
    include(conan_deps.cmake)
endif()

find_package(Threads REQUIRED)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # CMake cannot find the static library for Visual Studio so provide it
    set(LIBXML2_LIBRARY ${CONAN_LIBXML2_ROOT}/lib/libxml2_a.lib)
endif()
find_package(LibXml2 2.9 REQUIRED)

find_package(ZLIB REQUIRED)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(DIRENT_H_HASH_EXPECTED "ce4cc82a7e1b1a2d9df607c893fc95b03847783d271bce86bb97ee5f2c42b82f")
    if(EXISTS "${CMAKE_BINARY_DIR}/include/dirent.h")
        file(SHA256 "${CMAKE_BINARY_DIR}/include/dirent.h" DIRENT_H_HASH_FOUND)
    endif()
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/include/dirent.h" OR NOT "${DIRENT_H_HASH_FOUND}" STREQUAL "${DIRENT_H_HASH_EXPECTED}")
        message(STATUS "Downloading dirent.h from https://github.com/tronkko/dirent")
        file(DOWNLOAD https://github.com/tronkko/dirent/raw/1.23.2/include/dirent.h ${CMAKE_BINARY_DIR}/include/dirent.h
                EXPECTED_HASH SHA256=${DIRENT_H_HASH_EXPECTED}
                TLS_VERIFY ON)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4)
    # TODO: always enable when all warnings are fixed
    if (USE_VS_ANALYZE)
        add_compile_options(/analyze)
    else()
        add_compile_options(/WX)
    endif()
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Werror -Weverything)
    # we do not care about padding
    add_compile_options(-Wno-padded)
    # we do not care about backwards compatibility (started with Clang 14)
    add_compile_options(-Wno-declaration-after-statement)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Werror -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings -Wunreachable-code -Wpedantic -Wundef)
    # these warnings produce false positives before GCC 10
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "10.0.1")
        add_compile_options(-Wconversion -Wsign-conversion)
        # TODO: always enable when all warnings are fixed
        if (USE_GCC_ANALYZER)
            add_compile_options(-fanalyzer -Wno-error)
        endif()
    endif()
endif()

if (USE_SANITIZERS)
    # TODO: add more additional compiler-specific flags
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(SANITIZER_FLAGS -fsanitize=address -fsanitize=undefined)
        add_compile_options(${SANITIZER_FLAGS} -fno-omit-frame-pointer)
        add_definitions(-DNO_ASSERT)
        add_link_options(${SANITIZER_FLAGS})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # TODO: make this work
        # works only in 32-bit Release builds
        # add_compile_definitions(/fsanitize=address)
    endif()
endif()

if (MINWG32)
    add_compile_definitions(-DIN_LIBXML)
endif()

add_library(common-obj OBJECT common.c)
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_include_directories(common-obj SYSTEM PRIVATE ${CMAKE_BINARY_DIR}/include)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(common-obj PRIVATE -Wno-sign-conversion)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(common-obj PRIVATE -Wno-sign-conversion)
endif()
target_include_directories(common-obj SYSTEM PRIVATE ${ZLIB_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})

add_library(config-obj OBJECT config.c)
target_include_directories(config-obj SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})

add_executable(mame_regtest mame_regtest.c $<TARGET_OBJECTS:common-obj> $<TARGET_OBJECTS:config-obj>)
target_include_directories(mame_regtest SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(mame_regtest PRIVATE -Wno-format-truncation -Wno-sign-conversion)
endif()
target_link_libraries(mame_regtest PRIVATE Threads::Threads ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(mame_regtest PRIVATE ws2_32)
endif()

add_executable(create_report create_report.c $<TARGET_OBJECTS:common-obj> $<TARGET_OBJECTS:config-obj>)
target_include_directories(create_report SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(create_report PRIVATE -Wno-cast-qual -Wno-disabled-macro-expansion -Wno-double-promotion)
endif()
target_link_libraries(create_report PRIVATE Threads::Threads ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(create_report PRIVATE ws2_32)
endif()

add_executable(create_image_xml create_image_xml.c $<TARGET_OBJECTS:common-obj>)
target_include_directories(create_image_xml SYSTEM PRIVATE ${LIBXML2_INCLUDE_DIR} ${Iconv_INCLUDE_DIR})
target_link_libraries(create_image_xml PRIVATE Threads::Threads ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES})
if (WIN32)
    target_link_libraries(create_image_xml PRIVATE ws2_32)
endif()

include(processorcount.cmake)
include(clang_tidy.cmake)
include(iwyu.cmake)
include(cppcheck.cmake)