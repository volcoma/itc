file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_library(lib2 ${libsrc})

target_include_directories(lib2
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(lib2
    PRIVATE
		itc
)
set_target_properties(lib2 PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_compile_definitions(lib2 PUBLIC LIB2_BUILD)
