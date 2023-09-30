#include "MapReduce/rpc_service.h"
#include <cassert>
#include <system_error>
#include <vector>
#include <ylt/coro_rpc/coro_rpc_server.hpp>
#include <ylt/easylog.hpp>





int main(int argc, const char* argv[] ) {

    // FIXME: 动态加载文件位置 ./ mrsequentail lib/libwc.so pg*.txt
    if ( argc < 2 ) {
        ELOG_CRITICAL << " bin/master  MapReduce/pg*.txt  ";
        exit(1);
    }

    // easylog::set_min_severity(easylog::Severity::INFO);
    easylog::set_min_severity(easylog::Severity::ERROR);

    MasterService master{ 7 };
    master.init(argc, argv );

    /****************************/
    // ELOG_DEBUG << "num_map :"<< master.num_map;
    // ELOG_DEBUG << "num_reduce :"<< master.num_reduce;
    // ELOG_DEBUG << "cnt_map :" << master.cnt_map; 
    // ELOG_DEBUG << "cnt_reduce :" << master.cnt_reduce; 
    /****************************/

    coro_rpc::coro_rpc_server server( 8, 8000 );

    server.register_handler<&MasterService::allocateTask>( &master );
    server.register_handler<&MasterService::mapCompleted>( &master );
    server.register_handler<&MasterService::reduceCompleted>( &master );
    

    master.waitFunc(1).start( [](auto&&){} ); // 设置定时任务

    auto ret = server.start();
    assert( ret == std::errc{} );

    // sleep( 10000000 );
    return 0;

}