file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_library(lib1 ${libsrc})

target_include_directories(lib1
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(lib1
    PRIVATE
		itc
)
set_target_properties(lib1 PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include(target_warning_support)
set_warning_level(lib1 ultra)
