set(kcp_SRCS
    ikcp.c
    )


add_library(muduo_net_kcp ${kcp_SRCS})

install(TARGETS muduo_net_kcp DESTINATION lib)


set(HEADERS
    ikcp.h
    )


install(FILES ${HEADERS} DESTINATION include/muduo/net/kcp)






