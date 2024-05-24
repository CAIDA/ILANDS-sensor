add_executable(trace2grb trace2grb.c ../extern/cryptopANT.c)
target_link_libraries(trace2grb "${GRAPHBLAS_LIBRARIES}" "${OPENSSL_LIBRARIES}" trace)    
install(TARGETS trace2grb DESTINATION bin)