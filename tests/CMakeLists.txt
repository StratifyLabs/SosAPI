

set(DEPENDENCIES SosAPI SysAPI TestAPI FsAPI )

stratify_api_add_test_executable(SosAPI 32768 "${DEPENDENCIES}")



