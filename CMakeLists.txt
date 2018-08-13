cmake_minimum_required(VERSION 3.4.3)

project (itc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/utility")

option(BUILD_ITC_SHARED "Build as a shared library." ON)
option(BUILD_ITC_TESTS "Build the tests" ON)
option(BUILD_ITC_WITH_CODE_STYLE_CHECKS "Build with code style checks." ON)

if(BUILD_ITC_TESTS)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
endif()

if(BUILD_ITC_SHARED)
	set(BUILD_SHARED_LIBS ON)
endif()

if(BUILD_ITC_WITH_CODE_STYLE_CHECKS)
	set(BUILD_WITH_CODE_STYLE_CHECKS ON)
endif()

add_subdirectory(itc)

if(BUILD_ITC_TESTS)
	add_subdirectory(tests)
endif()
