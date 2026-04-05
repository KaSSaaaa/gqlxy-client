file(GLOB UNIT_TEST_SOURCES "*.cpp")

add_executable(gqlxy_client_unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(gqlxy_client_unit_tests PRIVATE gqlxy_client GTest::gtest_main)
target_include_directories(gqlxy_client_unit_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/tests
)
gtest_discover_tests(gqlxy_client_unit_tests)
