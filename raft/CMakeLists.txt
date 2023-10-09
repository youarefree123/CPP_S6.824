# 遍历当前目录和子目录
# file(GLOB_RECURSE SRC_DIR_LIST "./*.cpp")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# 只遍历当前目录
# file(GLOB  SRC_DIR_LIST "wc.cpp")
message("src---${PROJECT_SOURCE_DIR}")


add_library(
        raft
        # STATIC
        SHARED
        "raft.cpp"								
) 
target_link_libraries( raft pthread )

add_executable( test test.cpp )
target_link_libraries( test raft )


# target_include_directories (
#         mrpc PUBLIC 
#   ${3rd_PATH}/spdlog/include
#   ${INCLUDE_PATH}
# )


# target_link_libraries(
#   mrpc 
#   muduo_net
#   muduo_base
#   pthread
#   zookeeper_mt
# ) 


# add_subdirectory (tests) # 指定 编译子目录

