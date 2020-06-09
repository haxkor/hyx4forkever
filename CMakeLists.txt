cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(hyx VERSION 0.1.6 LANGUAGES C)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -DDEBUG -Wall -Wextra")

file(GLOB sources "*.c")

add_executable(hyx ${sources})
set_property(TARGET hyx PROPERTY C_STANDARD 11)

if(CMAKE_BUILD_TYPE=="Debug")
	target_compile_options(hyx PRIVATE "-Wall -Wextra -pedantic")
endif()

target_link_libraries(hyx pthread)
