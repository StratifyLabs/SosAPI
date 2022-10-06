set(LIBRARIES CryptoAPI SysAPI FsAPI ThreadAPI)
if(CMSDK_IS_LINK)
	list(APPEND LIBRARIES UsbAPI StratifyOS)
endif()
api2_add_library(
	NAME SosAPI
	DEPENDENCIES ${LIBRARIES})



