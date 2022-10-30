
option(SOS_API_USE_CRYPTO_API "Build Auth/Crypto features." ON)

if(SOS_API_USE_CRYPTO_API)
	set(LIBRARIES CryptoAPI)
	cmsdk2_check_version(
		NAME CryptoAPI
		VERSION ${CryptoAPI_VERSION}
		MINIMUM_VERSION 1.4.2)
else()
	set(LIBRARIES)
	cmsdk2_check_version(
		NAME API
		VERSION ${API_VERSION}
		MINIMUM_VERSION 1.6.4)
endif()

list(APPEND LIBRARIES SysAPI FsAPI ThreadAPI)
if(CMSDK_IS_LINK)
	list(APPEND LIBRARIES UsbAPI StratifyOS)
endif()

api2_add_library(
	NAME SosAPI
	DEPENDENCIES ${LIBRARIES}
	TARGETS TARGET_LIST)

foreach(TARGET ${TARGET_LIST})
	target_compile_definitions(${TARGET}
		PUBLIC
		SOS_API_USE_CRYPTO_API=$<IF:$<BOOL:${SOS_API_USE_CRYPTO_API}>,1,0>)
endforeach()





