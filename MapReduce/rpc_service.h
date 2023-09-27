#pragma once
#include <string>

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

inline Response allocateTask();