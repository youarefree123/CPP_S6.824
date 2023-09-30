#pragma once

#include <async_simple/coro/SpinLock.h>
#include <string>
#include <vector>
#include <list>
#include <queue>
#include <ylt/coro_rpc/coro_rpc_server.hpp>

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
    int num_file; // 总文件数，影响 reduce 中的 shuffle操作 所需要的文件数
    int num_reduce; // reduce 的数量， 影响 map操作， 生成 bucket 的数量
    int file_id; // map 阶段， file_id 指的是file_list的下标； reduce 阶段， file_id 代表 需要reduce的下标（reduce共有num_reduce个）
    std::string filename; // 仅在map阶段有用
};

// 本质就是一个FSM, 管理各种状态
class MasterService {
public:
    MasterService( int _num_reeduce = 7 ) 
      : num_reduce( _num_reeduce ) {}
    
    void init( int argc, const char* argv[] ); // 非RPC函数
    async_simple::coro::Lazy<Response> allocateTask(); // 每次给worker分配任务
    async_simple::coro::Lazy<void> mapCompleted( int fd ); // map 完成的通知
    async_simple::coro::Lazy<void> reduceCompleted( int fd ); // reduce 完成的通知

public:
    // 假设超过1s种没有反应的worker默认已经死了（ 真实场景中可能只是很慢，落盘前需要考虑一致性问题：先全部缓存，再统一落盘 ）
    int num_map; // map 数量（ 貌似没有用哎 ）
    int num_reduce; // reduce 的数量，影响分桶的数量
    std::queue<Response> map_ready_queue; // map 的就绪队列(待处理)，存Response纯属懒，不想重新定义新struct了
    std::list<Response> map_wait_queue; // map 的等待队列（正在处理）
    int num_map_completed = 0; // map 已完成数

    std::queue<Response> reduce_ready_queue; // reduce的就绪队列(待处理)，存Response纯属懒，不想重新定义新struct了
    std::list<Response> reduce_wait_queue; // reduce 的等待队列（正在处理）
    int num_reduce_completed = 0; // reduce 已完成数

    std::vector<const char*> file_list; // 需要处理的文件集合
    async_simple::coro::SpinLock lock; // 协程锁

    void getMapResponse( Response&, int );  // 封装 操作map的 报文
    void getReduceResponse( Response&, int );  // 封装 操作reduce的 报文
    void getWaitResponse( Response& );  // 封装 操作wait的 报文
    void getDoneResponse( Response& );  // 封装 操作wait的 报文

    async_simple::coro::Lazy<void> waitMap( int id );
    async_simple::coro::Lazy<void> waitReduce( int id );
    async_simple::coro::Lazy<void> waitFunc( int timestamp ) ;
    
};

struct KeyValue {
    std::string key, value;
};

// dlsym 返回void*， 要强制转换成原来的函数指针类型
using MapFunction = std::vector<KeyValue> (*)(std::string_view,
                                              std::string_view);
using ReduceFunction = std::string (*)( std::string_view, const std::vector<std::string>& );