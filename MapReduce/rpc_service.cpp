

#include "MapReduce/rpc_service.h"
#include <async_simple/coro/SyncAwait.h>
#include <ylt/easylog.hpp>


using async_simple::coro::syncAwait;

// 初始化要加载的文件列表, num_map 大小就是文件数量？！
void MasterService::init( int argc, const char* argv[] ) {
    // 加载文件名列表
    for( int i = 1; i < argc; i++ ) {  
        file_list.push_back( argv[i] );
        ELOG_INFO << argv[i] <<  " 文件加载完成 ";
    }
    // 设置num_map 大小
    num_map = file_list.size();
    
}

void MasterService::getMapResponse( Response& rsp ) {
    rsp.task_type = MAP;
    rsp.file_id = assigned_map++;
    rsp.filename = file_list[ rsp.file_id ];
    rsp.num_map = num_map;
    rsp.num_reduce = num_reduce;
}

// 封装 操作reduce的 报文
void MasterService::getReduceResponse( Response& rsp ) {
    rsp.task_type = REDUCE;
    rsp.file_id = assigned_reduce++;
    rsp.filename = ""; // reduce 不需要该字段
    rsp.num_map = num_map;
    rsp.num_reduce = num_reduce;
} 
void MasterService::getWaitResponse( Response& rsp ) {
    rsp.task_type = WAIT;

}
void MasterService::getDoneResponse( Response& rsp ) {
    rsp.task_type = DONE;
}

// 每次给worker分配任务, 先假设一定可以做好
async_simple::coro::Lazy<Response> MasterService::allocateTask(){
    Response rsp;

    // 如果map没做完，去做map, 如果 reduce 没做完，就去做reduce, 否则做down
    // syncAwait( lock.coLock() ); // 加锁 
    co_await lock.coLock();

    if( assigned_map < num_map ) {
        getMapResponse( rsp ); // map还没分发完, 分发map
    }
    else if( cnt_map != num_map ) {
        getWaitResponse( rsp );
    }
    else if ( assigned_reduce < num_reduce ) {
        getReduceResponse( rsp );
    }
    else if ( cnt_reduce != num_reduce ) {
        getWaitResponse( rsp );
    }
    else {
        getDoneResponse( rsp );
    }

    lock.unlock(); // 解锁
    
    // return rsp;
    co_return rsp;
} 


async_simple::coro::Lazy<void> MasterService::mapCompleted() {
    // syncAwait( lock.coLock() );
    co_await lock.coLock();
    ++cnt_map;
    lock.unlock();
}

async_simple::coro::Lazy<void> MasterService::reduceCompleted() {
    // syncAwait( lock.coLock() );
    co_await lock.coLock();
    ++cnt_reduce;
    lock.unlock();
}