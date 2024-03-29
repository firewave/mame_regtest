set(CONAN_SYSTEM_INCLUDES ON)
set(ENV{CONAN_USER_HOME} "${CMAKE_BINARY_DIR}")

set(CONAN_CMAKE_HASH_EXPECTED "5cdb3042632da3efff558924eecefd580a0e786863a857ca097c3d1d43df5dcd")
if(EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
    file(SHA256 "${CMAKE_BINARY_DIR}/conan.cmake" CONAN_CMAKE_HASH_FOUND)
endif()
if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake" OR NOT "${CONAN_CMAKE_HASH_FOUND}" STREQUAL "${CONAN_CMAKE_HASH_EXPECTED}")
    message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
    file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/0.18.1/conan.cmake" "${CMAKE_BINARY_DIR}/conan.cmake"
            EXPECTED_HASH SHA256=${CONAN_CMAKE_HASH_EXPECTED}
            TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

find_package(PythonInterp REQUIRED)

set(CONAN_VENV_PATH ${CMAKE_BINARY_DIR}/.conan-venv)

message(STATUS "Setting up Python virtuelenv")
execute_process(COMMAND ${PYTHON_EXECUTABLE} -m venv --upgrade-deps ${CONAN_VENV_PATH}
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "venv creation failed - ${PROC_RES}")
endif()

if (WIN32)
    set(CONAN_VENV_BIN ${CONAN_VENV_PATH}/Scripts)
    set(ENV{PATH} "${CONAN_VENV_BIN};$ENV{PATH}")
else()
    set(CONAN_VENV_BIN ${CONAN_VENV_PATH}/bin)
    set(ENV{PATH} "${CONAN_VENV_BIN}:$ENV{PATH}")
endif()

message(STATUS "Setting up conan Python package")
execute_process(COMMAND pip install --upgrade conan
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "conan installation failed - ${PROC_RES}")
endif()

conan_check(VERSION 1.0.0 REQUIRED)

# always remove the locks - we only use this internally and each configuration has its own folder
message(STATUS "Removing existing conan locks")
execute_process(COMMAND conan remove --locks
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "conan lock removal failed - ${PROC_RES}")
endif()

# workaround for "WARN: settings.yml is locally modified, can't be updated" false positive - only the default is ever created and it is never touched
if (EXISTS "${CMAKE_BINARY_DIR}/.conan/settings.yml.new")
    message(STATUS "Finishing settings.yml migration")
    file(RENAME "${CMAKE_BINARY_DIR}/.conan/settings.yml.new" "${CMAKE_BINARY_DIR}/.conan/settings.yml")
endif()

# TODO: make the build type configurable - "missing" might be enough for normal development
message(STATUS "Running conan")
if (MINGW)
    configure_file(${CMAKE_SOURCE_DIR}/default_with_cmake ${CMAKE_BINARY_DIR}/.conan/profiles/default_with_cmake COPYONLY)
    conan_cmake_run(CONANFILE conanfile.txt
            PROFILE default_with_cmake
            PROFILE_AUTO arch compiler compiler.version compiler.runtime compiler.libcxx compiler.toolset
            SETTINGS compiler.exception=seh
            SETTINGS compiler.threads=posix
            BUILD_TYPE "CONANFILE"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated
            UPDATE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    configure_file(${CMAKE_SOURCE_DIR}/default_with_cmake ${CMAKE_BINARY_DIR}/.conan/profiles/default_with_cmake COPYONLY)
    conan_cmake_run(CONANFILE conanfile.txt
            PROFILE default_with_cmake
            PROFILE_AUTO arch compiler compiler.version compiler.runtime compiler.libcxx compiler.toolset
            BUILD_TYPE "CONANFILE"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated
            UPDATE)
else()
    conan_cmake_run(CONANFILE conanfile.txt
            BUILD_TYPE "None"
            BASIC_SETUP NO_OUTPUT_DIRS
            BUILD outdated
            UPDATE)
endif()

message(STATUS "Removing conan build and source folders")
execute_process(COMMAND conan remove "*" --builds --src --force
        RESULT_VARIABLE PROC_RES)
if (NOT PROC_RES EQUAL 0)
    message(FATAL_ERROR "conan build and source folder removal failed - ${PROC_RES}")
endif()