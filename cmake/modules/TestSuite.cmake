# Allow to easily build test suites

# Define a new target property to hold the list of tests associated with a test
# suite. This property is named UNIT_TESTS to avoid confusion with the directory
# level property TESTS.
define_property(TARGET
	PROPERTY UNIT_TESTS
	BRIEF_DOCS "List of tests"
	FULL_DOCS "A list of the tests associated with a test suite"
)

macro(get_target_from_suite SUITE TARGET)
	set(${TARGET} "check-${SUITE}")
endmacro()

function(create_test_suite_with_parent_targets NAME)
	get_target_from_suite(${NAME} TARGET)

	add_custom_target(${TARGET}
		COMMENT "Running ${NAME} test suite"
		COMMAND cmake -E echo "PASSED: ${NAME} test suite"
	)

	foreach(PARENT_TARGET ${ARGN})
		if(TARGET ${PARENT_TARGET})
			add_dependencies(${PARENT_TARGET} ${TARGET})
		endif()
	endforeach()
endfunction()

macro(create_test_suite NAME)
	create_test_suite_with_parent_targets(${NAME} check-all check-extended)
endmacro()

set(TEST_RUNNER_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../templates/TestRunner.cmake.in")
function(add_test_runner SUITE NAME COMMAND)
	get_target_from_suite(${SUITE} SUITE_TARGET)
	set(TARGET "${SUITE_TARGET}-${NAME}")

	set(LOG "${NAME}.log")
	set(RUNNER "${CMAKE_CURRENT_BINARY_DIR}/run-${SUITE}-${NAME}.sh")
	list(JOIN ARGN " " ARGS)

	configure_file(
		"${TEST_RUNNER_TEMPLATE}"
		"${RUNNER}"
	)

	add_custom_target(${TARGET}
		COMMAND ${RUNNER}
		COMMENT "${SUITE}: testing ${NAME}"
		DEPENDS
			${COMMAND}
			${RUNNER}
	)
	add_dependencies(${SUITE_TARGET} ${TARGET})
endfunction()

function(add_test_to_suite SUITE NAME)
	add_executable(${NAME} EXCLUDE_FROM_ALL ${ARGN})
	add_test_runner(${SUITE} ${NAME} ${NAME})

	get_target_from_suite(${SUITE} TARGET)
	set_property(
		TARGET ${TARGET}
		APPEND PROPERTY UNIT_TESTS ${NAME}
	)
endfunction(add_test_to_suite)

function(add_boost_unit_tests_to_suite SUITE NAME)
	cmake_parse_arguments(ARG
		""
		""
		"TESTS"
		${ARGN}
	)

	get_target_from_suite(${SUITE} SUITE_TARGET)
	add_executable(${NAME} EXCLUDE_FROM_ALL ${ARG_UNPARSED_ARGUMENTS})
	add_dependencies("${SUITE_TARGET}" ${NAME})

	foreach(_test_source ${ARG_TESTS})
		target_sources(${NAME} PRIVATE "${_test_source}")
		get_filename_component(_test_name "${_test_source}" NAME_WE)
		add_test_runner(
			${SUITE}
			${_test_name}
			${NAME} -t "${_test_name}"
		)
		set_property(
			TARGET ${SUITE_TARGET}
			APPEND PROPERTY UNIT_TESTS ${_test_name}
		)
	endforeach()

	find_package(Boost 1.58 REQUIRED unit_test_framework)
	target_link_libraries(${NAME} Boost::unit_test_framework)

	# We need to detect if the BOOST_TEST_DYN_LINK flag is required
	include(CheckCXXSourceCompiles)
	set(CMAKE_REQUIRED_LIBRARIES Boost::unit_test_framework)

	check_cxx_source_compiles("
		#define BOOST_TEST_DYN_LINK
		#define BOOST_TEST_MAIN
		#include <boost/test/unit_test.hpp>
	" BOOST_TEST_DYN_LINK)

	if(BOOST_TEST_DYN_LINK)
		target_compile_definitions(${NAME} PRIVATE BOOST_TEST_DYN_LINK)
	endif(BOOST_TEST_DYN_LINK)
endfunction(add_boost_unit_tests_to_suite)
