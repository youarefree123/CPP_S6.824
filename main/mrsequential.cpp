#include "MapReduce/wc.h"
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <functional>
#include <sstream>
#include <iostream>
#include <string_view>

#define FILE_DIR "MapReduce/pg-being_ernest.txt"
#define SO_DIR "lib/libwc.so"

// typedef std::vector<KeyValue> 
using  MapFunction = std::vector<KeyValue> (*)( std::string_view, std::string_view );

int main() {
    //运行时从动态库中加载map及reduce函数(根据实际需要的功能加载对应的Func)
    void* handle = dlopen(SO_DIR, RTLD_LAZY);
    if (!handle) {
        // cerr << "Cannot open library: " << dlerror() << '\n';
        std::cout<<" so 不存在 "<<std::endl;
        exit(-1);
    }
    // std::cout<<" so 加载完成 "<<std::endl;

    auto mapTask =  (MapFunction) dlsym(handle, "mapTask");
    if (!mapTask) {
        std::cout << "Cannot load symbol 'mapTask': " << dlerror() <<'\n';
        dlclose(handle);
        exit(-1);
    }
    // std::cout<<" map 加载完成 "<<std::endl;

    // 读取文件
    std::ifstream file_input;
    file_input.open( FILE_DIR );
    if( !file_input.is_open() ) {
        std::cout<<" 文件不存在 " << std::endl;
        exit(1);
    }
    
    std::ostringstream tmp;
    tmp<<file_input.rdbuf();
    std::string contens = tmp.str();
    // std::cout<<" contens 加载完成 "<<std::endl;

    auto kv = mapTask( FILE_DIR, contens );
    // std::cout<<" 解析成功 " << std::endl;

    for( auto& it : kv ) {
        std::cout<<it.key<<" : "<<it.value<<std::endl;
    }

    return 0;
}