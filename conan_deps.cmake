set(CONAN_SYSTEM_INCLUDES ON)
set(ENV{CONAN_USER_HOME} "${CMAKE_BINARY_DIR}")

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/v0.15/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake")
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

find_package(PythonInterp REQUIRED)

set(CONAN_VENV_PATH ${CMAKE_BINARY_DIR}/.conan-venv)
if (WIN32)
    set(CONAN_VENV_BIN ${CONAN_VENV_PATH}/Scripts)
    set(ENV{PATH} "${CONAN_VENV_BIN};$ENV{PATH}")
else()
    set(CONAN_VENV_BIN ${CONAN_VENV_PATH}/bin)
    set(ENV{PATH} "${CONAN_VENV_BIN}:$ENV{PATH}")
endif()

execute_process(COMMAND ${PYTHON_EXECUTABLE} -m venv ${CONAN_VENV_PATH}
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "venv creation failed")
endif()

execute_process(COMMAND python -m pip install --upgrade pip
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "pip installation failed")
endif()

execute_process(COMMAND pip install --upgrade setuptools
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "setuptools installation failed")
endif()

execute_process(COMMAND pip install --upgrade conan
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "conan installation failed")
endif()

conan_check(VERSION 1.0.0 REQUIRED)

# always remove the locks - we only use this internally and each configuration has its own folder
execute_process(COMMAND conan remove --locks
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "conan lock removal failed")
endif()

# TODO: make the build type configurable - "missing" might be enough for normal development
if (MINGW)
    configure_file(${CMAKE_SOURCE_DIR}/default_with_cmake ${CMAKE_BINARY_DIR}/.conan/profiles/default_with_cmake COPYONLY)
    conan_cmake_run(CONANFILE conanfile.txt
            PROFILE default_with_cmake
            PROFILE_AUTO arch compiler compiler.version compiler.runtime compiler.libcxx compiler.toolset
            SETTINGS compiler.exception=seh
            SETTINGS compiler.threads=posix
            BUILD_TYPE "CONANFILE"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    configure_file(${CMAKE_SOURCE_DIR}/default_with_cmake ${CMAKE_BINARY_DIR}/.conan/profiles/default_with_cmake COPYONLY)
    conan_cmake_run(CONANFILE conanfile.txt
            PROFILE default_with_cmake
            PROFILE_AUTO arch compiler compiler.version compiler.runtime compiler.libcxx compiler.toolset
            BUILD_TYPE "CONANFILE"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated)
else()
    conan_cmake_run(CONANFILE conanfile.txt
            BUILD_TYPE "None"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated)
endif()