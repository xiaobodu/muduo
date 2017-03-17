set(tkcp_SRCS
    UdpMessage.cc
    UdpService.cc
    UdpSocket.cc
    TkcpSession.cc
    TkcpServer.cc
    Coding.cc
    Packet.cc
    TkcpClient.cc
    )


add_library(muduo_net_tkcp ${tkcp_SRCS})
target_link_libraries(muduo_net_tkcp muduo_net muduo_net_ikcp profiler)

add_library(muduo_net_tkcp11 ${tkcp_SRCS})
target_link_libraries(muduo_net_tkcp11 muduo_net_cpp11 muduo_net_ikcp)
set_target_properties(muduo_net_tkcp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")


install(TARGETS muduo_net_tkcp DESTINATION lib)
install(TARGETS muduo_net_tkcp11 DESTINATION lib)


set(HEADERS
    TkcpClient.h
    TkcpServer.h
    TkcpSession.h
    TkcpCallback.h
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/tkcp)

add_subdirectory(kcp)
add_subdirectory(tests)





