file(GLOB E2E_TEST_SOURCES "*.cpp")

add_executable(gqlxy_client_e2e_tests ${E2E_TEST_SOURCES})
target_link_libraries(gqlxy_client_e2e_tests PRIVATE
  gqlxy_client
  gqlxy_standalone_server
  nlohmann_json::nlohmann_json
  GTest::gtest
  Boost::beast
)
target_include_directories(gqlxy_client_e2e_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/tests
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${gqlxy-server_SOURCE_DIR}/include"
)

gtest_discover_tests(gqlxy_client_e2e_tests PROPERTIES TIMEOUT 60)
