
if(NOT DEFINED IS_SDK)
include(StratifyAPI)
if(SOS_IS_LINK)
	include(CryptoAPI)
	include(UsbAPI)
endif()
sos_sdk_include_target(SosAPI "${STRATIFYAPI_CONFIG_LIST}")
endif()
