# 遍历当前目录和子目录
# file(GLOB_RECURSE SRC_DIR_LIST "./*.cpp")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# 只遍历当前目录
# file(GLOB  SRC_DIR_LIST "wc.cpp")
message("src---${PROJECT_SOURCE_DIR}")


add_library(
        wc 
        # STATIC
        SHARED
        "wc.cpp"								
) 

add_library(
        mtiming 
        # STATIC
        SHARED
        "mtiming.cpp"								
) 

add_library(
        rtiming 
        # STATIC
        SHARED
        "rtiming.cpp"								
) 

add_library(
        nocrash 
        # STATIC
        SHARED
        "nocrash.cpp"								
) 

add_library(
        crash 
        # STATIC
        SHARED
        "crash.cpp"								
) 

add_executable( master master.cpp rpc_service.cpp )
# target_link_libraries( master pthread )

add_executable( worker worker.cpp  )
# target_link_libraries( worker pthread )

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

