#include "MapReduce/rpc_service.h"
#include <ylt/easylog.hpp>

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

// 每次给worker分配任务, 先假设一定可以做好
Response MasterService::allocateTask(){
    Response rsp;
    // 如果map没做完，去做map, 如果 reduce 没做完，就去做reduce, 否则做down
    if( cnt_map < num_map ) {
        rsp.task_type = MAP;
        rsp.file_id = cnt_map++;
        rsp.filename = file_list[ rsp.file_id ];
        rsp.num_map = num_map;
        rsp.num_reduce = num_reduce;
    }
    else if( cnt_reduce < num_reduce ) {
        rsp.task_type = REDUCE;
        rsp.file_id = cnt_reduce++;
        rsp.filename = ""; // reduce 不需要该字段
        rsp.num_map = num_map;
        rsp.num_reduce = num_reduce;
    }
    else {
        // 单机版中，这里只剩下done了，不会有wait
        rsp.task_type = DONE;
        rsp.file_id = -1;
        rsp.filename = ""; // reduce 不需要该字段
        rsp.num_map = num_map;
        rsp.num_reduce = num_reduce;
    }
    return rsp;
} 