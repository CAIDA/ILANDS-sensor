add_executable(pcap2grb pcap2grb.c ../extern/cryptopANT.c)
target_link_libraries(pcap2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}" "${PCAP_LIBRARIES}")
install(TARGETS pcap2grb DESTINATION bin)

add_executable(grb2pcap grb2pcap.c)
target_link_libraries(grb2pcap "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}" "${PCAP_LIBRARIES}")
install(TARGETS grb2pcap DESTINATION bin)