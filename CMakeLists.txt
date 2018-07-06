cmake_minimum_required(VERSION 3.4.3)

project (itc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")


option(BUILD_ITC_TESTS "Build the tests" ON)
option(BUILD_SHARED_LIBS "Build itc as a shared library." ON)

if(BUILD_ITC_TESTS)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

#include(clang_tidy_support)
#set_code_style(snake_case)

add_subdirectory(itc)

if(BUILD_ITC_TESTS)
	add_subdirectory(tests)
endif()
