# for each "example/x.cpp", generate target "x"
file(GLOB_RECURSE all_examples *.cpp)
foreach(v ${all_examples})
    string(REGEX MATCH "main/.*" relative_path ${v})
    # message(${relative_path})
    string(REGEX REPLACE "main/" "" target_name ${relative_path})
    string(REGEX REPLACE ".cpp" "" target_name ${target_name})

    add_executable(${target_name} ${v}) 
    target_link_libraries( ${${target_name}} wc )
endforeach()


