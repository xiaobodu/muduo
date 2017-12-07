if(HIREDIS_INCLUDE_DIR AND HIREDIS_LIBRARY)
  add_subdirectory(hiredis)
else()
  add_subdirectory(hiredis EXCLUDE_FROM_ALL)
endif()

if(THRIFT_COMPILER AND THRIFT_INCLUDE_DIR AND THRIFT_LIBRARY)
  add_subdirectory(thrift)
else()
  add_subdirectory(thrift EXCLUDE_FROM_ALL)
endif()


add_subdirectory(tkcp)
add_subdirectory(rudp)
add_subdirectory(kcp)
add_subdirectory(kcpnet)
