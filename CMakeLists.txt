cmake_minimum_required(VERSION 3.14)
project(lofeed LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-O3 -Wall -Wextra -Wpedantic)

add_executable(lofeed src/lofeed.cpp src/proto/itch.cpp src/book/book_replica.cpp)
target_include_directories(lofeed PRIVATE src)

add_executable(baseline src/baseline.cpp)
target_include_directories(baseline PRIVATE src)

add_executable(read_all src/read_all.cpp)
target_include_directories(read_all PRIVATE src)
