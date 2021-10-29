cmake_minimum_required(VERSION 3.12)
project(cache)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_CXX_FLAGS "-fsanitize=address,undefined ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-pthread ${CMAKE_CXX_FLAGS}")
add_executable(cache cache.cpp main.cpp)
