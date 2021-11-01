cmake_minimum_required (VERSION 3.12)

project(SosAPI CXX)
include(CTest)

add_subdirectory(library library)

option(SOS_API_IS_TEST "Enable test builds for SosAPI" OFF)
if(SOS_API_IS_TEST)
	add_subdirectory(tests tests)
endif()
