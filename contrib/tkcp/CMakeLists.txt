set(dog_SRCS
    UdpMessage.cc
    UdpService.cc
    UdpSocket.cc
    TkcpSession.cc
    TkcpServer.cc
    Coding.cc
    Packet.cc
    TkcpClient.cc
    kcp/ikcp.c
    )


add_library(muduo_net_dog ${dog_SRCS})
target_link_libraries(muduo_net_dog muduo_net profiler)


install(TARGETS muduo_net_dog DESTINATION lib)

set(HEADERS
    UdpMessage.h
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/dog)

add_subdirectory(tests)
