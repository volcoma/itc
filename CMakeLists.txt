cmake_minimum_required(VERSION 3.0)

project (itc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_ITC_TESTS "Build the tests" ON)

add_subdirectory(itc)

if(BUILD_ITC_TESTS)
	add_subdirectory(tests)
endif()
