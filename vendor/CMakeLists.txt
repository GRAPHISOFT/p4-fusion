add_subdirectory(libgit2)

set(BUILD_ONLY "core;s3" CACHE STRING "Filter build" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "Disable AWS SDK tests" FORCE)
add_subdirectory(aws-sdk-cpp)

add_subdirectory(minitrace)
set_target_properties(minitrace PROPERTIES C_STANDARD 99)
