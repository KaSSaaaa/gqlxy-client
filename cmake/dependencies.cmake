if(NOT TARGET gqlxy::core)
    include(FetchContent)
    FetchContent_Declare(
        gqlxy-core
        GIT_REPOSITORY https://github.com/KaSSaaaa/gqlxy-core.git
        GIT_TAG        02faf2da892525dc90677a4aff1c80a04f640151
    )
    FetchContent_MakeAvailable(gqlxy-core)
endif()

find_package(RPP CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(boost_beast CONFIG REQUIRED)
find_package(boost_url CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cppgraphqlgen CONFIG REQUIRED)
