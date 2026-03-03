cmake_minimum_required(VERSION 3.14)
project(ListSerializer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(list_serializer main.cpp)

if(WIN32)
    target_link_libraries(list_serializer PRIVATE ws2_32)
endif()

if(MSVC)
    target_compile_options(list_serializer PRIVATE /W4 /permissive-)
else()
    target_compile_options(list_serializer PRIVATE -Wall -Wextra -pedantic)
endif()