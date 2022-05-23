add_library(lexer STATIC lexer.cpp dfs-visualizer.cpp)

target_include_directories(
  lexer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lexer graphviz hash-set simple-stack)

add_unit_test(lexer-test lexer dfs-visualizer-tests.cpp)
