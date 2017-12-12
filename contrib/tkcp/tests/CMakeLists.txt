add_executable(TkcpServer_test TkcpServer_test.cc)
target_link_libraries(TkcpServer_test muduo_net_tkcp11
		      jemalloc
              #muduo_inspect
			  #profiler
		      )
add_executable(TkcpClient_test TkcpClient_test.cc)
target_link_libraries(TkcpClient_test muduo_net_tkcp11)
add_executable(roundtrip_test roundtrip_test.cc)
target_link_libraries(roundtrip_test muduo_net_tkcp11)
