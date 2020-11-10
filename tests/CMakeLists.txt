

set(DEPENDENCIES SosAPI SysAPI TestAPI CryptoAPI)
if(SOS_IS_LINK)
	list(APPEND DEPENDENCIES UsbAPI StratifyOS)
endif()
list(APPEND DEPENDENCIES FsAPI)


stratify_api_add_test_executable(SosAPI 32768 "${DEPENDENCIES}")



