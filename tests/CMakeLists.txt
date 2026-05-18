if (NOT BUILD_TESTING)
    return()
endif ()

find_package(GTest CONFIG REQUIRED)

enable_testing()
include(GoogleTest)

find_package(oatpp CONFIG REQUIRED)
find_package(oatpp-websocket CONFIG REQUIRED)
find_package(oatpp-openssl CONFIG REQUIRED)
find_package(gqlxy-server CONFIG REQUIRED)

add_subdirectory(unit)
add_subdirectory(e2e)

