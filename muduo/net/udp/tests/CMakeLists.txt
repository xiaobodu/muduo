
add_executable(UdpClientSocket_test UdpClientSocket_test.cc)
target_link_libraries(UdpClientSocket_test muduo_net_udp_cpp11 jemalloc)
add_executable(UdpServerSocket_test UdpServerSocket_test.cc)
target_link_libraries(UdpServerSocket_test muduo_net_udp_cpp11 muduo_inspect jemalloc)
