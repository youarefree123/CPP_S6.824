#pragma once

#include <string>
#include <string_view>
#include <vector>


struct KeyValue {
    std::string key, value;
};

// extern "C" 修饰，表示用c风格编译，为了加载动态库时函数名不会改变，不加的话函数名会有乱码后缀
extern "C" std::vector<KeyValue> mapTask( std::string_view filename, std::string_view contents );
extern "C" std::string reduceTask( std::string_view key, const std::vector<std::string>& value );