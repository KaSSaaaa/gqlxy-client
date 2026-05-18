if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(STATUS "Configuring subproject: ${PROJECT_NAME}")
endif()

file(GLOB_RECURSE SOURCES_FILES *.cpp)
add_library(gqlxy_client ${SOURCES_FILES})
target_compile_features(gqlxy_client PUBLIC cxx_std_20)

if(MSVC)
    target_compile_definitions(gqlxy_client PUBLIC _WIN32_WINNT=0x0A00)
    set_source_files_properties(
        gqlxy/client/internal/http/https_stream.cpp
        gqlxy/client/internal/ws/ws_stream.cpp
        gqlxy/client/internal/ws/wss_stream.cpp
        PROPERTIES COMPILE_FLAGS /bigobj
    )
endif()
target_include_directories(gqlxy_client
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(gqlxy_client
	PUBLIC
		RPP::rpp
		nlohmann_json::nlohmann_json
		gqlxy_core::gqlxy_core
	PRIVATE
		Boost::beast
		Boost::url
		OpenSSL::SSL
		OpenSSL::Crypto
)

add_library(gqlxy_client::gqlxy_client ALIAS gqlxy_client)

install(TARGETS gqlxy_client
	EXPORT gqlxy-clientTargets
)

install(EXPORT gqlxy-clientTargets
	FILE gqlxy-clientTargets.cmake
	NAMESPACE gqlxy_client::
	DESTINATION lib/cmake/gqlxy-client
)

if(EXISTS ${CMAKE_SOURCE_DIR}/include)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endif()
