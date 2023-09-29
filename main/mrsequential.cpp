/*
    单机版 MapReduce， 先Map， 将结果存入临时队列， 然后再统计
*/



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

int main( int argc, const char** argv ) {
    
    // FIXME: 动态加载文件位置 ./ mrsequentail lib/libwc.so pg*.txt
    if ( argc < 3 ) {
        ELOG_CRITICAL << " bin/mrsequential lib/libwc.so  MapReduce/pg*.txt  ";
        exit(1);
    }

    const char* SO_DIR = argv[1]; // so 位置
    std::vector<const char*> file_list; // file 位置
    for( int i = 2; i < argc; i++ ) {  
        file_list.push_back( argv[i] );
    }

    auto [ Map, Reduce ] = loadPlugin( SO_DIR ); // 函数导入

    // 遍历每个文件，做一次map，并入中间缓存
    std::vector<KeyValue> intermediate;
    std::string content;
    for( auto filename : file_list ) {

        loadFile(content, filename );
        std::vector<KeyValue> tmp = Map( filename, content );
        if( tmp.size() <= 0 ) {
            ELOG_ERROR << std::string{filename} <<" 解析失败 ";
        }
        ELOG_INFO << std::string{filename} << "解析成功 content 大小 : "<< content.size();
        intermediate.insert( intermediate.end(), tmp.begin(), tmp.end() );
    
    }
    
    ELOG_INFO << " 中间文件大小: "<< intermediate.size();


    auto partition = [&]( int low, int high ) -> int {
        auto pivot = intermediate[high];
        int i = (low - 1);
        for (int j = low; j <= high - 1; ++j) {
            if (intermediate[j].key < pivot.key ) {
                i++;
                std::swap(intermediate[i], intermediate[j]);
            }
        }
        std::swap(intermediate[i + 1], intermediate[high]);
        return (i + 1);
    };

    auto quicksort = [&]( int low, int high ) {
        std::stack<int> stack;
        stack.push(low);
        stack.push(high);

        while (!stack.empty()) {
            high = stack.top();
            stack.pop();
            low = stack.top();
            stack.pop();
            int pivotIndex = partition(low, high);
            if (pivotIndex - 1 > low) {
                stack.push(low);
                stack.push(pivotIndex - 1);
            }
            if (pivotIndex + 1 < high) {
                stack.push(pivotIndex + 1);
                stack.push(high);
            }
        }
    };

    // 对中间文件排序，然后reduce， 直接调sort 会爆栈， 需要手写非递归版
    quicksort(0, intermediate.size()-1 );
    // std::sort(intermediate.begin(), intermediate.end(),
    //      [](const KeyValue &lhs, const KeyValue &rhs) {
    //         return lhs.key <= rhs.key;
    //      });

    ELOG_INFO << " 中间文件排序完成: ";

    // 打开一个文件，写结果
    std::ofstream outfile("mr-wc",std::ofstream::trunc); // 每次都清空文件后再写
    if( !outfile.is_open() ) {
        ELOG_CRITICAL<<"mr-wc 文件创建失败 ";
        exit(1);
    }

    ELOG_INFO<<"mr-wc 文件创建成功 ";

    // 执行reduce
    size_t p = 0, q = 1, n = intermediate.size();
    std::string result;
    while( q <= n ) {
        while( q < n && intermediate[q-1].key == intermediate[q].key ) { ++q; }
        std::string values;
        while( p < q ) { 
            values.append( intermediate[p].value ); // 为了统一reduce接口, 就不直接生成结果文件了
            ++p;
        }
        q = p+1;
        // ELOG_INFO << intermediate[p-1].key << " : " << values;
        std::string output = Reduce( intermediate[p-1].key, values );
        outfile << intermediate[p-1].key << ":" << output << "\n";
    }
    
    outfile.close();

    return 0;
}