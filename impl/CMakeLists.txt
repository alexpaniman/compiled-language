add_executable(language language.cpp)

target_include_directories(language PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(language parser lexer graphviz)
