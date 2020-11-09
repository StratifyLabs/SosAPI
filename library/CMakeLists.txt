
set(LIBRARIES FsAPI ThreadAPI ChronoAPI VarAPI SysAPI CryptoAPI)
if(SOS_IS_LINK)
	list(APPEND LIBRARIES UsbAPI StratifyOS)
endif()

stratifyapi_add_api_library(SosAPI "${LIBRARIES}")



