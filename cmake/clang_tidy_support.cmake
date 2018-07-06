find_program(
	CLANG_TIDY_EXE
	NAMES "clang-tidy"
	DOC "Path to clang-tidy executable"
)

macro(set_clang_tidy_args ARGS_LIST)
	if(CLANG_TIDY_EXE)
		message(STATUS "===================================")
		message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
		message(STATUS "clang-tidy command line: ${ARGS_LIST}")
		message(STATUS "===================================")
		
		set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}"
				"${ARGS_LIST}"
			)
		set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
		set(CMAKE_CXX_CLANG_TIDY "${DO_CLANG_TIDY}")
	endif()
endmacro()

macro(set_target_clang_tidy_args ARG1, ARGS_LIST)
	if(CLANG_TIDY_EXE)
		message(STATUS "===================================")
		message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
		message(STATUS "clang-tidy command line: ${ARGS_LIST}")
		message(STATUS "===================================")

		set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}"
				"${ARGS_LIST}"
			)
		set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

		set_target_properties(${ARG1} PROPERTIES
			CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
		)
	endif()
endmacro()


macro(set_code_style)
set(preset ${ARGV0})

if(preset MATCHES "snake_case")
set(CLANG_TIDY_CHECKS_CONF "\
-config={ \
Checks: '-*,readability-identifier-naming', \
CheckOptions: [\
{ key: readability-identifier-naming.ClassCase, value: lower_case },\
{ key: readability-identifier-naming.ParameterCase, value: lower_case }, \
{ key: readability-identifier-naming.FunctionCase, value: lower_case },\
{ key: readability-identifier-naming.VariableCase, value: lower_case },\
{ key: readability-identifier-naming.MemberCase, value: lower_case },\
{ key: readability-identifier-naming.PrivateMemberCase, value: lower_case },\
{ key: readability-identifier-naming.ProtectedMemberCase, value: lower_case },\
{ key: readability-identifier-naming.PublicMemberCase, value: lower_case },\
{ key: readability-identifier-naming.PrivateMemberSuffix, value: _ },\
{ key: readability-identifier-naming.ProtectedMemberSuffix, value: _ }\
]}")
else()
	set(CLANG_TIDY_CHECKS_CONF ${preset})
endif()

set_clang_tidy_args("${CLANG_TIDY_CHECKS_CONF}")
endmacro()


macro(set_target_code_style)
set(target ${ARGV0})
set(preset ${ARGV1})

if(preset MATCHES "snake_case")
set(CLANG_TIDY_CHECKS_CONF "\
-config={ \
Checks: '-*,readability-identifier-naming', \
CheckOptions: [\
{ key: readability-identifier-naming.ClassCase, value: lower_case },\
{ key: readability-identifier-naming.ParameterCase, value: lower_case }, \
{ key: readability-identifier-naming.FunctionCase, value: lower_case },\
{ key: readability-identifier-naming.VariableCase, value: lower_case },\
{ key: readability-identifier-naming.MemberCase, value: lower_case },\
{ key: readability-identifier-naming.PrivateMemberCase, value: lower_case },\
{ key: readability-identifier-naming.ProtectedMemberCase, value: lower_case },\
{ key: readability-identifier-naming.PublicMemberCase, value: lower_case },\
{ key: readability-identifier-naming.PrivateMemberSuffix, value: _ },\
{ key: readability-identifier-naming.ProtectedMemberSuffix, value: _ }\
]}")
else()
	set(CLANG_TIDY_CHECKS_CONF ${preset})
endif()

set_target_clang_tidy_args(${target} "${CLANG_TIDY_CHECKS_CONF}")
endmacro()
