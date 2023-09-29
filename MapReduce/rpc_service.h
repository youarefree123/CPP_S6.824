#pragma once

#include <async_simple/coro/SpinLock.h>
#include <string>
#include <vector>

enum TaskType {
    MAP = 0,
    REDUCE,
    WAIT,
    DONE
};

// 特别说明
// 在Map操作中，表示每个文件的id， 但是在Reduce中，表示的是每个 bucket 的id， bucket的数量就是num_reduce的大小
struct Response {
    enum TaskType task_type;
    int num_map; // map的数量，影响 reduce 中的 shuffle操作 所需要的文件数
    int num_reduce; // reduce 的数量， 影响 map操作， 生成 bucket 的数量
    int file_id; 
    std::string filename;
    
};

// 本质就是一个FSM, 管理各种状态
class MasterService {
public:
    MasterService( int _num_reeduce = 7 ) 
      : num_reduce( _num_reeduce ) {}
    
    void init( int argc, const char* argv[] ); // 非RPC函数
    Response allocateTask(); // 每次给worker分配任务
    void mapCompleted(); // map 完成的通知
    void reduceCompleted(); // reduce 完成的通知

public:
    int num_map; // map的数量
    int assigned_map = 0; // 已分配的map id 
    int cnt_map = 0; // 已完成的map数量
    int num_reduce; // reduce 的数量
    int assigned_reduce = 0; // 已分配的 reduce id 
    int cnt_reduce = 0; // 已完成的reduce数量
    std::vector<const char*> file_list; // 需要处理的文件集合
    async_simple::coro::SpinLock lock;

    void getMapResponse( Response& );  // 封装 操作map的 报文
    void getReduceResponse( Response& );  // 封装 操作reduce的 报文
    void getWaitResponse( Response& );  // 封装 操作wait的 报文
    void getDoneResponse( Response& );  // 封装 操作wait的 报文
    
};