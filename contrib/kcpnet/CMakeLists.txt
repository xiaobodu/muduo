set(kcpnet_SRCS
   #KcpConntion.cc
    )


add_library(muduo_net_kcpnet ${kcpnet_SRCS})
target_link_libraries(muduo_net_kcpnet muduo_net)

add_library(muduo_net_kcpnet_cpp11 ${kcpnet_SRCS})
target_link_libraries(muduo_net_kcpnet_cpp11 muduo_net_cpp11)
set_target_properties(muduo_net_kcpnet_cpp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")


install(TARGETS muduo_net_kcpnet DESTINATION lib)


set(HEADERS
   # KcpConntion.h
)


#install(FILES ${HEADERS} DESTINATION include/muduo/net/kcpnet)

add_subdirectory(tests)






