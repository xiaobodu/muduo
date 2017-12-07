set(udp_SRCS
    UdpSocket.cc
    UdpServerSocket.cc
    UdpClientSocket.cc
    )


add_library(muduo_net_udp ${udp_SRCS})
target_link_libraries(muduo_net_udp muduo_net)

add_library(muduo_net_udp_cpp11 ${udp_SRCS})
target_link_libraries(muduo_net_udp_cpp11 muduo_net_cpp11)
set_target_properties(muduo_net_udp_cpp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")


install(TARGETS muduo_net_udp DESTINATION lib)
install(TARGETS muduo_net_udp_cpp11 DESTINATION lib)


set(HEADERS
    UdpSocket.h
    UdpServerSocket.h
    UdpClientSocket.h
)


install(FILES ${HEADERS} DESTINATION include/muduo/net/udp)


add_subdirectory(tests)




