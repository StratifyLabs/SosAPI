cmake_minimum_required (VERSION 3.12)
cmsdk2_minimum_required(2.1.2)

project(SosAPI
	LANGUAGES CXX
	VERSION 1.4.4)
include(CTest)

add_subdirectory(library library)

option(SOS_API_IS_TEST "Enable test builds for SosAPI" OFF)
if(SOS_API_IS_TEST)
	add_subdirectory(tests tests)
endif()
