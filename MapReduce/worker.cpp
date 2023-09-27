#include "MapReduce/wc.h"
#include <algorithm>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <functional>
#include <sstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <stack>
#include <ylt/easylog.hpp>
#include <ylt/easylog/record.hpp>

// dlsym 返回void*， 要强制转换成原来的函数指针类型
using MapFunction = std::vector<KeyValue> (*)(std::string_view,
                                              std::string_view);
using  ReduceFunction = std::string (*)( std::string_view, std::string_view );


// 加载Map 和 Reduce
std::pair<MapFunction, ReduceFunction> loadPlugin( const char* so_path ) {
    //运行时从动态库中加载map及reduce函数(根据实际需要的功能加载对应的Func)
    void* handle = dlopen( so_path, RTLD_LAZY);
    if (!handle) {
        ELOG_CRITICAL<<" so 不存在 ";
        exit(-1);
    }
    ELOG_INFO << " so 加载完成 ";

    auto Map =  (MapFunction) dlsym(handle, "mapTask");
    if (!Map) {
        dlclose(handle);
        ELOG_CRITICAL<< "Cannot load symbol 'mapTask': " << dlerror();
        exit(-1);
    }
    ELOG_INFO << " mapTask 加载完成 ";
    
    auto Reduce =  (ReduceFunction) dlsym(handle, "reduceTask");
    if (!Reduce) {
        dlclose(handle);
        ELOG_CRITICAL<< "Cannot load symbol 'reduceTask': " << dlerror();
        exit(-1);
    }
    ELOG_INFO << " reduceTask 加载完成 ";

    return std::make_pair(Map, Reduce);

}

// 加载文件内容
void loadFile( std::string& content, const char* filename ) {
    // 读取文件, 并导入字符流
    std::ifstream file_input;
    file_input.open( filename );
    if( !file_input.is_open() ) {
        ELOG_CRITICAL<<std::string{filename}<<" 文件不存在 ";
        exit(1);
    }

    std::ostringstream tmp;
    tmp<<file_input.rdbuf();
    file_input.close();
    content = tmp.str();
    ELOG_INFO << std::string{filename} << ": content 加载完成 ";

}


void worker( MapFunction Map, ReduceFunction Reduce ) {
    
}


int main ( int argc, const char* argv[] ) {
    // FIXME: 动态加载文件位置 ./ mrsequentail lib/libwc.so pg*.txt
    if ( argc < 2 ) {
        ELOG_CRITICAL << " bin/worker lib/libwc.so ";
        exit(1);
    }

    auto [ Map, Reduce ] = loadPlugin( argv[1] ); // 函数导入
    worker( Map, Reduce );

}