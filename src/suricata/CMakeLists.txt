add_executable(json2grb json2grb.c ../extern/cryptopANT.c ../extern/cJSON.c ../extern/yyjson.c)
target_link_libraries(json2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}" "${PCAP_LIBRARIES}")
install(TARGETS json2grb DESTINATION bin)

# Unused.
if(0)
if(EXISTS "${SURICATA_SOURCE_DIR}")
    add_library(suricata2grb SHARED suricata2grb.c ../extern/cJSON.c ../extern/cryptopANT.c)
    target_compile_options(suricata2grb PRIVATE -DSURICATA_PLUGIN)
    target_include_directories(suricata2grb PRIVATE "${SURICATA_SOURCE_DIR}")
    target_link_libraries(suricata2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}")
endif()
endif()