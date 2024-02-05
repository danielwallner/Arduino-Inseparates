# Mostly from: https://google.github.io/googletest/quickstart-cmake.html

cmake_minimum_required(VERSION 3.14)
project(InseparatesTest)

cmake_policy(SET CMP0135 NEW)

# GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

if (MSVC)
	add_compile_options(/W4 /WX)
else()
	add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_definitions(-DUNIT_TEST=1)

set(COMMON_SOURCES
	../src/Inseparates.h
	../src/Inseparates.cpp
	../src/FastTime.h
	../src/ProtocolUtils.h
	../src/DebugUtils.h
	../src/DebugUtils.cpp

	../src/ProtocolBeo36.h
	../src/ProtocolDatalink80.h
	../src/ProtocolDatalink86.h
	../src/ProtocolESI.h
	../src/ProtocolNEC.h
	../src/ProtocolRC5.h
	../src/ProtocolSIRC.h
	../src/ProtocolTechnicsSC.h

	../src/ProtocolUART.h

	Dummies.h
	Dummies.cpp
)

add_executable(debug_ir
	${COMMON_SOURCES}

	DebugIR.cpp
)

add_executable(debug_uart
	${COMMON_SOURCES}

	DebugUART.cpp
)

add_executable(test_all
	${COMMON_SOURCES}

	TestBeo36.cpp
	TestDatalink.cpp
	TestESI.cpp
	TestNEC.cpp
	TestRC5.cpp
	TestSIRC.cpp
	TestTechnicsSC.cpp

	TestUART.cpp
)

target_link_libraries(test_all
	GTest::gtest_main
	GTest::gmock_main
)

include(GoogleTest)
gtest_discover_tests(test_all)
