#pragma once 

#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SpinLock.h>
#include <cstddef>
#include <cstdint>
#include <math.h>
#include <memory>
#include <utility>
#include <random>
#include <ylt/coro_rpc/coro_rpc_client.hpp>
#include <ylt/coro_rpc/coro_rpc_server.hpp>
#include <vector>
#include "raft/common.h"

class Raft;

using coro_rpc::coro_rpc_client;
using coro_rpc::coro_rpc_server;
using async_simple::coro::Lazy;


using Raft_Ptr = std::unique_ptr<Raft>; 
using Client_Ptr = std::unique_ptr<coro_rpc_client>;
using Server_Ptr = std::unique_ptr<coro_rpc_server>;

class Persister;

enum STATE {
    Follower = 0,
    Candidate,
    Leader
};

struct Entry {
    size_t index; // 日志位置
    size_t term; // 日志提交时的周期
    std::string cmd;
};

// example RequestVote RPC arguments structure.
// field names must start with capital letters!
//
struct RequestVoteArgs {
    size_t term; // 候选人的term号
    size_t candidateId; // 请求投票的候选人id
    size_t lastLogIndex; // 候选人最后一条日志的位置索引
    size_t lastLogTerm; // 候选人最后一条日志的term号
};


//
// example RequestVote RPC reply structure.
// field names must start with capital letters!
//
struct RequestVoteReply {
    size_t term; // 当前的最新term号， for candidate to update itself
    bool voteGranted; // 该请求是否收到了选票 true means candidate received vote
};




class Raft {

public:
    
    Raft( const size_t& _me ) : me( _me ) {
        // 初始化时插入一条起始日志
        logs.emplace_back( 0, -1, "" );
    }
    ~Raft() = default;
    std::string test( std::string str ) { return str; }
    inline std::pair<int,bool> GetState() const; // 获取当前节点状态
    bool sendRequestVote(int id, RequestVoteArgs args, RequestVoteReply& reply); // rpc的封装，发送投票请求
    Lazy<RequestVoteReply> RequestVote( RequestVoteArgs args ); // rpc: 提供请求投票服务
    Lazy<void> ticker(); // 超时触发下一轮选举
    void appendEntries(); 
    inline void resetElectionTime(); // 重制选举超时时间
    inline void resetHeartBeatTime(); // 心跳包超时时间
    inline bool heartBeatTimeOut() const { return Now() >= this->heartBeatTime; }
    inline bool electionTimeOut() const { return Now() >= this->electionTime; }
    void leaderInit(); // 新leader需要做的一些初始化工作
    void turnTo( STATE state ); // 用于raft的状态切换
    void doElection(); // 选举



public:
    async_simple::coro::SpinLock lock; // 协程锁
    // std::vector<Client_Ptr> peers; // 与所有节点的服务端通信的客户端
    std::vector<std::string> peers; // 存储所有raft等ip端口
    std::string port; // 本raft所包含的服务器端口

public:
    Persister *persister ;         // Object to hold this peer's persisted state
    size_t me;                         // this peer's index into peers[]
	int32_t dead = 0;                  // set by Kill()

    /* 需要在RPC的response前更新的状态 */
    STATE state = Follower; // 默认初始状态是follower
    bool is_alive = true; // 表示raft是否存活
    size_t heartBeatTime = 0; // 下一次发送心跳包的时间
    size_t electionTime = 0; // raft 超过此时间就会开启选举逻辑

    size_t currentTerm = 0;    // raft 能观察到的 最新term号, 
    size_t votedFor = -1;    // 当前term，获得该节点票的id，默认为none， -1（无符号的最大值）
    std::vector<Entry> logs; // 日志

    /*    不需要持久化的状态：Volatile， 所有server都需要      */ 
    size_t commitIndex = 0; // 已知提交的最高日志条目的索引
    size_t lastApplied = 0; // 已知应用到状态机中的最高日志条目的索引, 如果小于commitIndex，则会在合适的时候应用日志

    /* Volatile ： leader 特有的状态 */
    std::vector<int> nextIndex; // 需要发送给followers的下一条日志索引
    std::vector<int> matchIndex; // 每个follower的已经复制完毕的最高日志索引

};

//
// the service or tester wants to create a Raft server. the ports
// of all the Raft servers (including this one) are in peers[]. this
// server's port is peers[me]. all the servers' peers[] arrays
// have the same order. persister is a place for this server to
// save its persistent state, and also initially holds the most
// recent saved state, if any. applyCh is a channel on which the
// tester or service expects Raft to send ApplyMsg messages.
// Make() must return quickly, so it should start goroutines
// for any long-running work.
// 初始化一个raft对象，同时开启定时任务ticker，用于超时后的选举
void Make( const std::vector<Raft_Ptr>& servers, size_t me, Persister *persister );