include(ProcessorCount)
ProcessorCount(NPROC)
if(NPROC EQUAL 0)
    message(FATAL_ERROR "could not get processor count")
endif()
math(EXPR NPROC_HALF "${NPROC} / 2")

message(STATUS "NPROC=${NPROC}")
message(STATUS "NPROC_HALF=${NPROC_HALF}")