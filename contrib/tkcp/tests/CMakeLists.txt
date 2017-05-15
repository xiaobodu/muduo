add_executable(UdpSocket_test UdpSocket_test.cc)
target_link_libraries(UdpSocket_test muduo_net_tkcp11)
add_executable(UdpService_test UdpService_test.cc)
target_link_libraries(UdpService_test muduo_net_tkcp11)
add_executable(TkcpServer_test TkcpServer_test.cc)
target_link_libraries(TkcpServer_test muduo_net_tkcp11
		      #jemalloc
		      )
add_executable(TkcpClient_test TkcpClient_test.cc)
target_link_libraries(TkcpClient_test muduo_net_tkcp11)
add_executable(roundtrip_test roundtrip_test.cc)
target_link_libraries(roundtrip_test muduo_net_tkcp11)
