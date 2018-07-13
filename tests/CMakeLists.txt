message(STATUS "Enabled itc tests.")

file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

add_executable(tests ${libsrc})

target_link_libraries(tests PUBLIC itc)

set_target_properties(tests PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

include(target_warning_support)
set_warning_level(tests ultra)
