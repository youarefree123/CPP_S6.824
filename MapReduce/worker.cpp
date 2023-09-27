#include "MapReduce/wc.h"
#include "MapReduce/rpc_service.h"
#include <algorithm>
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Sleep.h>
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
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SyncAwait.h>
#include <ylt/coro_rpc/coro_rpc_client.hpp>

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

//对每个字符串求hash找到其对应要分配的reduce线程
int ihash(std::string_view str, int num_reduce ){
    int sum = 0;
    for(int i = 0; i < str.size(); i++){
        sum += (str[i] - '0');
    }
    return sum % num_reduce;
}


async_simple::coro::Lazy<void> doMap( MapFunction Map, const Response& response ) {
    std::string content;
    loadFile(content, response.filename.data() );
    std::vector<KeyValue> intermediate = Map( response.filename, content );
    
    // 每个 do_map 产生 num_reduce 个 临时文件 
    std::vector< std::ofstream > file_streams( response.file_id );
    for( int i = 0; i < response.num_reduce; i++ ) {
        std::string filename = "mr-"+std::to_string(i)+"-"+std::to_string(response.file_id);
        file_streams[i].open( filename,std::ofstream::trunc  ); // 每次都清空文件后再写
        if( !file_streams[i].is_open() ) {
            ELOG_CRITICAL << filename << " was not created.";
            exit(1);
        }
        ELOG_INFO << filename <<" was be created.";
    }

    // 将所有的kv对都存入对应的文件中, 然后关闭
    for( auto& it : intermediate ) {
        file_streams[ ihash(it.key, response.num_reduce) ] << it.key <<":"<<it.value<<"\n";
    }

    for( auto& it : file_streams ) {
        it.close();
    }

    co_return;

}

async_simple::coro::Lazy<void> doReduce( ReduceFunction Reduce, const Response& response ) {
    
    // shuffle 操作，将所有kv都存入临时文件中
    std::vector<KeyValue> intermediate;
    for( int i = 0; i < response.num_map; i++ ) {
        std::string filename = "mr-" + std::to_string( response.file_id ) + "-" + std::to_string(i);
        std::ifstream tmp_file( filename );
        if( !tmp_file.is_open() ) {
            ELOG_CRITICAL << filename << " was not open.";
            exit(1);
        }
        ELOG_INFO << filename <<" was be opened.";
        
        std::string line;
        while (std::getline(tmp_file, line)) {
            int n = line.size();
            int pos = line.find(':');
            std::string_view key( line.begin(), line.begin()+pos );
            std::string_view value( line.begin()+pos+1, line.end() );
            intermediate.emplace_back( key,value );
        }
        tmp_file.close(); // 用完及时关闭文件
    }

    // 现在的value是1的集合，还需要排序后进行一波reduce
    std::sort( intermediate.begin(), intermediate.end(), 
    []( const KeyValue& lhs, const KeyValue& rhs ) {
        return lhs.key < rhs.key;
    } );

    // 打开一个文件，写结果
    std::ofstream outfile("mr-out-" + std::to_string(response.file_id),std::ofstream::trunc); // 每次都清空文件后再写
    if( !outfile.is_open() ) {
        ELOG_CRITICAL<<"mr-out-" << std::to_string(response.file_id)<<"文件创建失败 ";
        exit(1);
    }

    // 执行reduce
    size_t p = 0, q = 1, n = intermediate.size();
    
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

    co_return;
}


async_simple::coro::Lazy<void> worker( MapFunction Map, ReduceFunction Reduce ) {

    coro_rpc::coro_rpc_client client;
    co_await client.connect( "", "8000" );

    // 循环等待任务，并执行
    while( true ) {

        auto response = co_await client.call<allocateTask>();
        if( !response.has_value() ) {
            ELOG_ERROR << "allocateTask no response";
        }
        else {
            switch ( response.value().task_type ) {
                case MAP:
                    co_await doMap( Map, response.value() ); break;
                case REDUCE:
                    co_await doReduce( Reduce, response.value() ); break;
                case WAIT:
                    // 等待1s
                    co_await async_simple::coro::sleep( std::chrono::duration<int, std::ratio<1, 1>>(1) );
                    break;
                case DONE:
                    co_return;
                default:
                    ELOG_CRITICAL << " error type" << response.value().task_type;
                    exit(1);
            }

        }

    }
    
}


int main ( int argc, const char* argv[] ) {
    // FIXME: 动态加载文件位置 ./ mrsequentail lib/libwc.so pg*.txt
    if ( argc < 2 ) {
        ELOG_CRITICAL << " bin/worker lib/libwc.so ";
        exit(1);
    }

    auto [ Map, Reduce ] = loadPlugin( argv[1] ); // 函数导入
    async_simple::coro::syncAwait( worker( Map, Reduce ) ) ;

}