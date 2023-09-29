#include "MapReduce/rpc_service.h"
#include <cassert>
#include <system_error>
#include <vector>
#include <ylt/coro_rpc/coro_rpc_server.hpp>
#include <ylt/easylog.hpp>

// 异步定时任务，监听 任务是否完成，完成后让master退出
async_simple::coro::Lazy<void> waitFunc( int timestamp, const MasterService& master ) {
    while( true ) {
        co_await coro_io::sleep_for( std::chrono::duration< int, std::ratio<1,1> >(timestamp) );
        
        if( master.cnt_reduce == master.num_reduce ) {
            // 任务已经完成，master需要退出
            ELOG_INFO << " MapReduce Completed! ";
            exit(0);
        }
    }
    
    // 如果此时 ass
}



int main(int argc, const char* argv[] ) {

    // FIXME: 动态加载文件位置 ./ mrsequentail lib/libwc.so pg*.txt
    if ( argc < 2 ) {
        ELOG_CRITICAL << " bin/master  MapReduce/pg*.txt  ";
        exit(1);
    }

    MasterService master{ 7 };
    master.init(argc, argv );

    /****************************/
    // ELOG_DEBUG << "num_map :"<< master.num_map;
    // ELOG_DEBUG << "num_reduce :"<< master.num_reduce;
    // ELOG_DEBUG << "cnt_map :" << master.cnt_map; 
    // ELOG_DEBUG << "cnt_reduce :" << master.cnt_reduce; 
    /****************************/

    coro_rpc::coro_rpc_server server( 12, 8000 );

    server.register_handler<&MasterService::allocateTask>( &master );
    server.register_handler<&MasterService::mapCompleted>( &master );
    server.register_handler<&MasterService::reduceCompleted>( &master );
    

    waitFunc(1,master).start( [](auto&&){} ); // 设置定时任务

    auto ret = server.start();
    assert( ret == std::errc{} );

    sleep( 10000000 );
    return 0;

}