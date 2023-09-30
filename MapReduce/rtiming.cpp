#include "MapReduce/rtiming.h"
#include <async_simple/coro/Sleep.h>
#include <async_simple/coro/SyncAwait.h>
#include <csignal>
#include <iostream>
#include <fstream>
#include <string_view>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <cstring>
#include <sstream>

int nparallel(const std::string& phase) {
    // 创建一个文件，以便其他工作进程可以看到我们同时运行
    pid_t pid = getpid();
    std::string myfilename = "mr-worker-" + phase + "-" + std::to_string(pid);
    std::ofstream file(myfilename);
    if (!file) {
        throw std::runtime_error("Failed to create file: " + myfilename);
    }
    file.close();

    // 检查是否有其他并行工作进程在运行
    DIR* dir = opendir(".");
    if (!dir) {
        throw std::runtime_error("Failed to open directory");
    }

    struct dirent* entry;
    std::vector<int> pids;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        std::string pattern = "mr-worker-" + phase + "-";

        if (name.find(pattern) == 0) {
            int xpid;
            std::sscanf(name.c_str(), (pattern + "%d").c_str(), &xpid);

            if (kill(xpid, 0) == 0) {
                // 如果kill()成功，表示进程仍然存活
                pids.push_back(xpid);
            }
        }
    }
    closedir(dir);

    // async_simple::coro::syncAwait( async_simple::coro::sleep( std::chrono::duration<int, std::ratio<1, 1>>(1) ) ) ;
    sleep(1);  
                   
    // 删除创建的文件
    std::remove(myfilename.c_str());

    return pids.size();
}


std::vector<KeyValue> mapTask(std::string_view filename, std::string_view contents) {
    std::vector<KeyValue> kva;
    kva.push_back(KeyValue{"a", "1"});
    kva.push_back(KeyValue{"b", "1"});
    kva.push_back(KeyValue{"c", "1"});
    kva.push_back(KeyValue{"d", "1"});
    kva.push_back(KeyValue{"e", "1"});
    kva.push_back(KeyValue{"f", "1"});
    kva.push_back(KeyValue{"g", "1"});
    kva.push_back(KeyValue{"h", "1"});
    kva.push_back(KeyValue{"i", "1"});
    kva.push_back(KeyValue{"j", "1"});
    return kva;
}

std::string reduceTask(std::string_view key, const std::vector<std::string>& values) {
    int n = nparallel( "reduce" ); // Placeholder for parallel reduce

    std::string val = std::to_string(n);

    return val;
}