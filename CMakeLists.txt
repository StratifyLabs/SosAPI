cmake_minimum_required (VERSION 3.12)

if(NOT SOS_SDK_PATH)
	set(SOS_SDK_PATH $ENV{SOS_SDK_PATH})
endif()

set(CMAKE_MODULE_PATH ${SOS_SDK_PATH}/cmake)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_MODULE_PATH}/sos-toolchain.cmake)
include(sos-sdk)

if(NOT DEFINED API_IS_SDK)
	if(SOS_IS_LINK)
		include(StratifyOS)
		include(UsbAPI)
	endif()
	include(CryptoAPI)
endif()

set(LOCAL_NAME SosAPI)
project(${LOCAL_NAME} CXX)
include(CTest)

add_subdirectory(library library)
if(SOS_IS_TEST)
	add_subdirectory(tests tests)
endif()
