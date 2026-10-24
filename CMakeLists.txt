cmake_minimum_required(VERSION 3.14)
project(PathTracerFS65 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
add_executable(${PROJECT_NAME} PathTracerFS65.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
