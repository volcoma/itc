file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(target_name itc)

add_library(${target_name} ${libsrc})

target_include_directories(${target_name}
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
target_link_libraries(${target_name} PUBLIC Threads::Threads)

include(target_warning_support)
set_warning_level(${target_name} ultra)

#include(target_code_style_support)
#set_code_style(itc lower_case check_headers "${extra_flags}")
