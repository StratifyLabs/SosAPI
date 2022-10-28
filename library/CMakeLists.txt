
option(SOS_API_USE_CRYPTO_API "Build Auth/Crypto features." ON)

if(SOS_API_USE_CRYPTO_API)
	set(LIBRARIES CryptoAPI)
else()
	set(LIBRARIES)
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





