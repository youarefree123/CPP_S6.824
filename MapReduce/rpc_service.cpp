

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
    // 设置map的就绪队列
    for( int i = 0; i < file_list.size(); i++ ) {
        Response rsp;
        getMapResponse(rsp, i);
        map_ready_queue.push( rsp );
    }

    num_map = map_ready_queue.size();
    ELOG_INFO << " nums of  map_ready_queue : "<< map_ready_queue.size();

    // 设置 reduce 就绪队列
    for( int i = 0; i < num_reduce; i++ ) {
        Response rsp;
        getReduceResponse(rsp, i );
        reduce_ready_queue.push( rsp );
    }
    ELOG_INFO << " nums of  reduce_ready_queue : "<< reduce_ready_queue.size();
    
}

void MasterService::getMapResponse( Response& rsp, int id ) {
    rsp.task_type = MAP;
    rsp.file_id = id;
    rsp.filename = file_list[ rsp.file_id ];
    rsp.num_file = file_list.size();
    rsp.num_reduce = num_reduce;
}

// 封装 操作reduce的 报文
void MasterService::getReduceResponse( Response& rsp, int id ) {
    rsp.task_type = REDUCE;
    rsp.file_id = id;
    rsp.filename = ""; // reduce 不需要该字段
    rsp.num_file = file_list.size();
    rsp.num_reduce = num_reduce;
} 
void MasterService::getWaitResponse( Response& rsp ) {
    rsp.task_type = WAIT;

}
void MasterService::getDoneResponse( Response& rsp ) {
    rsp.task_type = DONE;
}

async_simple::coro::Lazy<void> MasterService::waitMap( int id ) {
    while( true ) {
        co_await coro_io::sleep_for( std::chrono::duration< int, std::ratio<1,2> >(1) );
    }
    // 任务从等待队列中移除，然后重新加入就绪队列
    for( auto it = map_wait_queue.begin(); it != map_wait_queue.end(); it++ ) {
        if( it->file_id == id ) {
            map_ready_queue.push( *it );
            map_wait_queue.erase( it );
            break;
        }
    }
}

async_simple::coro::Lazy<void> MasterService::waitReduce( int id ) {
    while( true ) {
        co_await coro_io::sleep_for( std::chrono::duration< int, std::ratio<1,2> >(1) );
    }
    // 任务从等待队列中移除，然后重新加入就绪队列
    for( auto it = reduce_wait_queue.begin(); it != reduce_wait_queue.end(); it++ ) {
        if( it->file_id == id ) {
            reduce_ready_queue.push( *it );
            reduce_wait_queue.erase( it );
            break;
        }
    }
}


// 每次给worker分配任务, 先假设一定可以做好, 然后假设有的可能会宕机（不宕机的一定完成）
async_simple::coro::Lazy<Response> MasterService::allocateTask(){
    Response rsp;
    // 做异步化，这里现在是同步模式 
    // https://github.com/alibaba/yalantinglibs/blob/main/website/docs/zh/coro_rpc/coro_rpc_introduction.md
    // 如果map没做完，去做map, 如果 reduce 没做完，就去做reduce, 否则做down
    // syncAwait( lock.coLock() ); // 加锁保平安
    co_await lock.coLock();

    if( num_map_completed < num_map ) { // map 没做完，做map

        if( map_ready_queue.empty() ) { // 如果没有可执行的任务，等待一下
            getWaitResponse(rsp);
        }
        else { // 有map任务，执行，并设置定时器，超时的话就把该任务继续移入就绪队列，移除等待队列
            rsp = map_ready_queue.front();
            map_ready_queue.pop();
            waitMap( rsp.file_id ).start([](auto&&){} ) ; // 设置超时定时任务
        }
        
    }

    else if ( num_reduce_completed < num_reduce ) { // 此时在做reduce

        if( reduce_ready_queue.empty() ) { // 等待
            getWaitResponse(rsp);
        }
        else {
            rsp = reduce_ready_queue.front();
            reduce_ready_queue.pop();
            waitReduce( rsp.file_id ).start( [](auto&&){} );
        }

    }
    else {
        // 结束事件
        getDoneResponse(rsp);
    }

    lock.unlock(); // 解锁

    co_return rsp;
} 


async_simple::coro::Lazy<void> MasterService::mapCompleted( int fd ) {
    // syncAwait( lock.coLock() );
    co_await lock.coLock();
    ++num_map_completed;
    for( auto it = map_wait_queue.begin(); it != map_wait_queue.end(); it++ ) {
        if( it->file_id == fd ) {
            map_wait_queue.erase( it );
            break;
        }
    }

    lock.unlock();
}

async_simple::coro::Lazy<void> MasterService::reduceCompleted( int fd ) {
    // syncAwait( lock.coLock() );
    co_await lock.coLock();
    ++num_reduce_completed;

    for( auto it = reduce_wait_queue.begin(); it != reduce_wait_queue.end(); it++ ) {
        if( it->file_id == fd ) {
            reduce_wait_queue.erase( it );
            break;
        }
    }

    lock.unlock();
}

// 异步定时任务，监听 任务是否完成，完成后让master退出
async_simple::coro::Lazy<void> MasterService::waitFunc( int timestamp ) {
    while( true ) {
        co_await coro_io::sleep_for( std::chrono::duration< int, std::ratio<1,2> >(timestamp) );
        
        ELOG_INFO << " now map_com: " << num_map_completed << ", reduce_com:"<< num_reduce_completed;

        if( num_reduce_completed == num_reduce ) {
            // 任务已经完成，master需要退出
            ELOG_INFO << " MapReduce Completed! ";
            exit(0);
        }
    }
    
    // 如果此时 ass
}