find_package(ftxui CONFIG REQUIRED)

add_executable(repl main.cpp)
target_include_directories(repl PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(repl PRIVATE gqlxy::client ftxui::component)
