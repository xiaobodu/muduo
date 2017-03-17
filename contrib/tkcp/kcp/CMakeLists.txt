set(tkcp_SRCS
    ikcp.c
    )


add_library(muduo_net_ikcp ${tkcp_SRCS})



install(TARGETS muduo_net_ikcp DESTINATION lib)


set(HEADERS
    ikcp.h
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/tkcp/kcp)






