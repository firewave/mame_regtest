find_program(CPPCHECK NAMES cppcheck)
message(STATUS "CPPCHECK=${CPPCHECK}")
if (CPPCHECK)
    set(CPPCHECK_PROJECT_ARG --project=${CMAKE_BINARY_DIR}/compile_commands.json)
    # TODO: make --inconclusive optional
    # TODO: add as additional command - ${CPPCHECK} --check-config ${CPPCHECK_PROJECT_ARG}
    # TODO: make suppressions and libraries configurable
    add_custom_target(run-cppcheck ${CPPCHECK} --enable=all --suppress=missingIncludeSystem --suppress=unmatchedSuppression --library=zlib --inconclusive ${CPPCHECK_PROJECT_ARG})
endif()