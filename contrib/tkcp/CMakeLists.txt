set(tkcp_SRCS
    TkcpConnection.cc
    TkcpServer.cc
    Coding.cc
    Packet.cc
    TkcpClient.cc
    Fec.cc
    ikcp.c
    )


add_library(muduo_net_tkcp ${tkcp_SRCS})
target_link_libraries(muduo_net_tkcp muduo_net_udp muduo_net)

add_library(muduo_net_tkcp11 ${tkcp_SRCS})
target_link_libraries(muduo_net_tkcp11 muduo_net_udp_cpp11 muduo_net_cpp11)
set_target_properties(muduo_net_tkcp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")


install(TARGETS muduo_net_tkcp DESTINATION lib)
install(TARGETS muduo_net_tkcp11 DESTINATION lib)


set(HEADERS
    TkcpClient.h
    TkcpServer.h
    TkcpConnection.h
    TkcpCallback.h
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/tkcp)

add_subdirectory(tests)





