file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_library(itc ${libsrc})

target_include_directories(itc
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_definitions(itc PRIVATE ITC_API_EXPORTS)
set_target_properties(itc PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
	WINDOWS_EXPORT_ALL_SYMBOLS ON
    POSITION_INDEPENDENT_CODE ON
)


include(target_warning_support)
set_warning_level(itc ultra)
