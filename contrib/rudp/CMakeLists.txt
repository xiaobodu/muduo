set(rudp_SRCS
    UdpSocket.cc
    UdpServerSocket.cc
    UdpClientSocket.cc
    ReliableRtoCalc.cc
    ReliableByteBuffer.cc
    ReliableFlusher.cc
    ReliableBuffer.cc
    ReliableConnection.cc
    ReliableUdpConnection.cc
    ReliableUdpServer.cc
    cpu.cc
    test.cc
    )


add_library(muduo_net_rudp ${rudp_SRCS})
target_link_libraries(muduo_net_rudp muduo_net)

add_library(muduo_net_rudp_cpp11 ${rudp_SRCS})
target_link_libraries(muduo_net_rudp_cpp11 muduo_net_cpp11)
set_target_properties(muduo_net_rudp_cpp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")


install(TARGETS muduo_net_rudp DESTINATION lib)
install(TARGETS muduo_net_rudp DESTINATION lib)


set(HEADERS
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/rudp)

add_subdirectory(tests)





