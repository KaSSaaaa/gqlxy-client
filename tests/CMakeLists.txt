if (NOT BUILD_TESTING)
    return()
endif ()

find_package(GTest CONFIG REQUIRED)

enable_testing()
include(GoogleTest)

include(FetchContent)
set(BUILD_STANDALONE_SERVER ON CACHE BOOL "" FORCE)
set(ENABLE_VCPKG_BOOTSTRAP OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        gqlxy-server
        GIT_REPOSITORY https://github.com/KaSSaaaa/gqlxy-server.git
        GIT_TAG 0fa753e50b209c5ac23aeb2e0f6ef5ff85aa3977
)
FetchContent_MakeAvailable(gqlxy-server)

add_subdirectory(unit)
add_subdirectory(e2e)

