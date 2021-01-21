find_program(RUN_CLANG_TIDY NAMES run-clang-tidy run-clang-tidy-11 run-clang-tidy-10 run-clang-tidy-9 run-clang-tidy-8)
message(STATUS "RUN_CLANG_TIDY=${RUN_CLANG_TIDY}")
if (RUN_CLANG_TIDY)
    add_custom_target(run-clang-tidy ${RUN_CLANG_TIDY} -q -p=${CMAKE_BINARY_DIR} -j ${NPROC_HALF})
endif()