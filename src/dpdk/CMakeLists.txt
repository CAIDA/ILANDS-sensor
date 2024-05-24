add_executable(dpdk2grb dpdk2grb.c ../extern/cryptopANT.c)
target_compile_options(dpdk2grb PRIVATE ${LIBDPDK_CFLAGS})
target_link_libraries(dpdk2grb ${LIBDPDK_LDFLAGS} "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}")
install(TARGETS dpdk2grb DESTINATION bin)
