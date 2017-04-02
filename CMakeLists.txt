cmake_minimum_required(VERSION 3.7)
project(webserver-handout)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp test.cpp test.h)
add_executable(untitled ${SOURCE_FILES})