add_executable(iplist2grb iplist2grb.c ../extern/cryptopANT.c)
target_link_libraries(iplist2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}")
install(TARGETS iplist2grb DESTINATION bin)

add_executable(gbdump gbdump.c)
target_link_libraries(gbdump "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}")
install(TARGETS gbdump DESTINATION bin)

add_executable(gbdeserialize gbdeserialize.c)
target_link_libraries(gbdeserialize "${GRAPHBLAS_LIBRARIES}")

if(HAVE_CSV_H)
    add_executable(csv2grb csv2grb.c ../extern/cryptopANT.c)
    target_link_libraries(csv2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}" csv)
    install(TARGETS csv2grb DESTINATION bin)
endif()


add_executable(uint2ip uint2ip.c)

add_executable(makecache makecache.c ../extern/cryptopANT.c)
target_link_libraries(makecache "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}")
