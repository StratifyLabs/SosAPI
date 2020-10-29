
if(NOT DEFINED IS_SDK)
include(StratifyAPI)
sos_sdk_include_target(SosAPI "${STRATIFYAPI_CONFIG_LIST}")
endif()
