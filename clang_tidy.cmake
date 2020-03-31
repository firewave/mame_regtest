find_program(CLANG_TIDY NAMES clang-tidy clang-tidy-9 clang-tidy-8)
message(STATUS "CLANG_TIDY=${CLANG_TIDY}")
if (CLANG_TIDY)
    # TODO: read files from compilation database
    add_custom_target(run-clang-tidy ${CLANG_TIDY} -p=${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/*.c)
endif()