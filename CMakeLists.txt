# lhf - CMake Build File

set(LHF_VERSION 0.4.0)

cmake_minimum_required(VERSION 3.23)
project(lhf VERSION ${LHF_VERSION} LANGUAGES CXX)
include_guard(GLOBAL)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Avoid warning about fetchcontent
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
	cmake_policy(SET CMP0135 NEW)
endif()

set(MINIMUM_GCC_VERSION 11.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lhf INTERFACE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_ROOT "${SRC_ROOT}/include")
set(EXAMPLE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/examples")
set(TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/tests")

# Add include directory for headers.
target_include_directories(lhf INTERFACE
	$<BUILD_INTERFACE:${INCLUDE_ROOT}>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set(
	ENABLE_PERFORMANCE_METRICS
	ON
	CACHE BOOL
	"Enable performance measuring routines in the project (for compiling tests and examples).")

set(
	DISABLE_INTEGRITY_CHECKS
	OFF
	CACHE BOOL
	"Disable integrity checks (such as on set registration) and possibly introduce undefined behaviour. (for compiling tests and examples).")

set(
	ENABLE_DEBUG
	ON
	CACHE BOOL
	"Enable debugging routines in the project (for compiling tests and examples).")

set(
	ENABLE_PARALLEL
	OFF
	CACHE BOOL
	"Enable parallelization (for compiling tests and examples). Cannot be used with ENABLE_TBB.")

set(
	ENABLE_TBB
	OFF
	CACHE BOOL
	"Enable TBB-based parallelization (for compiling tests and examples). Cannot be used with ENABLE_PARLLEL.")

set(
	ENABLE_EVICTION
	OFF
	CACHE BOOL
	"Enables the ability to evict sets (for compiling tests and examples).")

set(
	ENABLE_TESTS
	OFF
	CACHE BOOL
	"Enable compilation of tests for the project.")

set(
	ENABLE_EXAMPLES
	ON
	CACHE BOOL
	"Enable compilation of examples for the project.")

add_compile_options(
	"-Wall"
	"-Wextra"
	"-Wno-unused-function"
	"-g")

file(GLOB INCLUDE_FILES "${INCLUDE_ROOT}/lhf/*.hpp")
file(GLOB EXAMPLE_SOURCES "${SRC_ROOT}/examples/*.cpp")
file(GLOB TEST_SOURCES "${SRC_ROOT}/tests/*.cpp")

if(ENABLE_PARALLEL AND ENABLE_TBB)
	message(FATAL_ERROR "ENABLE_PARALLEL and ENABLE_TBB are mutually exclusive." )
elseif(ENABLE_PARALLEL)
	target_compile_definitions(lhf INTERFACE LHF_ENABLE_PARALLEL)
elseif(ENABLE_TBB)
	find_package(TBB REQUIRED)
	target_compile_definitions(lhf INTERFACE LHF_ENABLE_TBB)
	target_link_libraries(lhf INTERFACE TBB::tbb)
endif()

if(ENABLE_PERFORMANCE_METRICS)
	target_compile_definitions(lhf INTERFACE LHF_ENABLE_PERFORMANCE_METRICS)
endif()

if(ENABLE_DEBUG)
	target_compile_definitions(lhf INTERFACE LHF_ENABLE_DEBUG)
endif()

if(DISABLE_INTEGRITY_CHECKS)
	target_compile_definitions(lhf INTERFACE LHF_DISABLE_INTEGRITY_CHECKS)
endif()

# Compile every single source file in the example directory to an executable

if(ENABLE_EXAMPLES)
	foreach(EXAMPLE_SOURCE ${EXAMPLE_SOURCES})
		get_filename_component(EXAMPLE_NAME ${EXAMPLE_SOURCE} NAME_WE)
		add_executable("${EXAMPLE_NAME}" "${EXAMPLE_SOURCE}")
		target_include_directories("${EXAMPLE_NAME}" PRIVATE "${INCLUDE_ROOT}")
		target_link_libraries("${EXAMPLE_NAME}" lhf)
		set_target_properties(${EXAMPLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${EXAMPLE_OUTPUT_DIR}")
	endforeach()
endif()

# Build tests (using googletest)

if(ENABLE_TESTS)
	# Include googletest
	include(FetchContent)

	# Do not install gtest when running make install
	set(INSTALL_GTEST OFF)

	FetchContent_Declare(
		googletest
		URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
	)

	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)

	enable_testing()
	include(GoogleTest)

	foreach(TEST_SOURCE ${TEST_SOURCES})
		get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
		set(TEST_NAME "test_${TEST_NAME}")
		add_executable("${TEST_NAME}" "${TEST_SOURCE}")
		target_include_directories("${TEST_NAME}" PRIVATE "${INCLUDE_ROOT}")
		target_link_libraries("${TEST_NAME}" lhf)
		target_link_libraries("${TEST_NAME}" GTest::gtest_main)
		set_target_properties(${TEST_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}")
		gtest_discover_tests("${TEST_NAME}")
	endforeach()

endif()

# Installation Setup.
#
# This is a header-only library at the moment, so we have only added in the
# options for installing header files.
#
# Not much thought has been put into the below lines, but it should
# theoretically work with find_package(lhf) and you'll be able to get the
# header inclusions working.
#
# It installs by default to /usr/local on a linux machine, as far as I'm aware.

target_sources(lhf
	INTERFACE FILE_SET HEADERS
	BASE_DIRS ${INCLUDE_ROOT}
	FILES ${INCLUDE_FILES})

install(TARGETS lhf
	EXPORT lhfTargets
	FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT lhfTargets
	FILE lhfConfig.cmake
	NAMESPACE lhf::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lhf)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/lhfConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/lhfTargetsVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lhf)

export(EXPORT lhfTargets
	FILE ${CMAKE_CURRENT_BINARY_DIR}/lhfConfig.cmake
	NAMESPACE lhf::)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/lhfConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/lhfConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lhf)

write_basic_package_version_file(
	${PROJECT_BINARY_DIR}/lhfTargetsVersion.cmake
	VERSION ${LHF_VERSION}
	COMPATIBILITY AnyNewerVersion
	ARCH_INDEPENDENT)