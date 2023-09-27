#include "MapReduce/rpc_service.h"
#include <vector>
#include <ylt/coro_rpc/coro_rpc_server.hpp>

// 本质就是一个FSM, 管理各种状态
struct Master {
    int num_map; // map的数量
    int cnt_map; // 已完成的map数量
    int num_reduce; // reduce 的数量
    int cnt_reduce; // 已完成的reduce数量
    std::vector<const char*> file_list; // 需要处理的文件集合
}



int main() {

    coro_rpc::coro_rpc_server server( 12, 8000 );
    server.register_handler<allocateTask>( );
    server.start();

}