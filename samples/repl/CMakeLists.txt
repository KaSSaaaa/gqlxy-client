find_package(ftxui CONFIG REQUIRED)

add_executable(repl main.cpp)
target_link_libraries(repl PRIVATE gqlxy::client ftxui::component)
