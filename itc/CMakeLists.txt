file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_library(itc ${libsrc})

target_include_directories(itc
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
set_target_properties(itc PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(itc PUBLIC Threads::Threads)

include(target_warning_support)
set_warning_level(itc ultra)

include(target_code_style_support)
set_code_style(itc lower_case check_headers "${extra_flags}")
