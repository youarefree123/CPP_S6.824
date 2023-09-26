cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

add_compile_options(-fPIC)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("Setting default build type to Release")
endif()

# 设置 bin 目录
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
# 设置Lib 目录
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib")


project(s6.824 VERSION 0.0.1 LANGUAGES C CXX)

include_directories(${PROJECT_SOURCE_DIR})      

add_subdirectory(MapReduce)
add_subdirectory(main)

# add_subdirectory(./test)

# add_subdirectory(./example)
