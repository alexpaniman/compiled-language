cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(lang VERSION 1.0)

add_subdirectory(c-lib)

add_subdirectory(lexer)

# add_subdirectory(parser)
