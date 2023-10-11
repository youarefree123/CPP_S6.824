#include <async_simple/coro/FutureAwaiter.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <string>
#include <ylt/coro_rpc/impl/coro_rpc_client.hpp>
#include <ylt/easylog.hpp>
#include "common.h"
#include "raft/persister.h"
#include "raft/raft.h"



// return currentTerm and whether this server
// believes it is the leader.
std::pair<int,bool> Raft::GetState() const {
    
    int64_t term;
    bool is_leader; 
    //  TODO 
    return std::make_pair( term, is_leader );
} 




bool Raft::sendAppendEntries( int id, AppendEntryArgs args, AppendEntryReply& reply ) {
    coro_rpc_client client;
    syncAwait(client.connect( "localhost", this->peers[id], milli_duration(DURATION_TIME) ) );
    auto ok = syncAwait( client.call_for<&Raft::appendEntries>(milli_duration( DURATION_TIME ), args) ); 
        
    if( !ok  ) {
        return false;
    }
    reply = std::move( ok.value() );
    return true;
}




Lazy<void> Raft::appendTo( int id ) {
    co_await lock.coLock();
    if( state != Leader ) {
        ELOG_DEBUG << me << " server 状态已经不是Leader了";
        lock.unlock();
        co_return;
    }
    auto args = AppendEntryArgs{};
    args.term = currentTerm;
    args.leaderId = me;
    args.preLogIndex = nextIndex[id] - 1; // 前一个
    args.preLogTerm = 0;
    args.leaderCommit = commitIndex;

    int64_t pre_index = nextIndex[id] - 1; // 上一条日志的index
    pre_index = transfer( pre_index ); // 该索引的日志在logs中的日志
    if( pre_index < 0 ) {
        lock.unlock();
        co_return;
    }

    args.preLogIndex = logs[pre_index].index;
    args.preLogTerm = logs[pre_index].term;

    // 复制日志
    std::vector<Entry> entries( logs.begin()+pre_index+1, logs.end() );
    args.entries = std::move( entries );

    lock.unlock();

    AppendEntryReply reply{};
    auto ok = sendAppendEntries( id, args, reply );
    if( !ok ) {
        co_return;
    }

    co_await lock.coLock();

    // 一些异常状况的处理
    if( currentTerm != args.term || state != Leader || reply.term < currentTerm ) {
        goto APD_RET;
    }

    // 如果收到了任期大于自身的消息，说明有了新Leader，自己需要转为follower
    if( reply.term > currentTerm ) {
        currentTerm = reply.term;
        votedFor = -1;
        // persist();  // FIXME
        turnTo( Follower );
        goto APD_RET;
    }

    // 如果id 节点 复制成功，修改 状态
    if( reply.success ) {
        nextIndex[id] = args.preLogIndex + args.entries.size() + 1;
        matchIndex[id] = args.preLogIndex + args.entries.size();
        toCommit();
        goto APD_RET;
    }

    // 回退 
    nextIndex[id] -= 1;

    if( nextIndex[id] < 1 ) {
        nextIndex[id] = 1;
    }

APD_RET:
    lock.unlock();
    co_return;

}

int64_t Raft::transfer( int64_t index ) {
    auto begin = logs.begin()->index;
        auto end = logs.end()->index;
        if( index < begin || index > end ) {
            return -1;
        }
        return index - begin;
}

// 复制日志
Lazy<AppendEntryReply> Raft::appendEntries( AppendEntryArgs args ) {
    AppendEntryReply reply{};
    co_await lock.coLock();
    
    
    // 过期日志，直接返回
    if( args.term < currentTerm ) {
        reply.term = currentTerm;
        reply.success = false;
        // goto RPC_APD;
        lock.unlock();
        // persist(); FIXME
        co_return reply;
    }

    // 状态转为follower
    if( args.term > currentTerm ) {
        currentTerm = args.term;
        votedFor = -1;
        turnTo( Follower );
    }

    if( state != Follower ) {
        turnTo( Follower );
    }
    
    reply.success = true;
    reply.term = currentTerm;
    resetElectionTime();

   


    auto index = transfer( args.preLogIndex );

    if ( args.preLogIndex < logs.begin()->index ) {
        reply.success = false;
        // goto RPC_APD;
        lock.unlock();
        // persist(); FIXME
        co_return reply;
    }

    if ( args.preLogIndex > logs.back().index ) {
        reply.success = false;
        // goto RPC_APD;
        lock.unlock();
        // persist(); FIXME
        co_return reply;
    }

    if( index < 0 ) {
        // goto RPC_APD;
        lock.unlock();
        // persist(); FIXME
        co_return reply;
    }

    // 不是一个任期，失败
    if( logs[index].term != args.preLogTerm ) {
        reply.success = false;
        // goto RPC_APD;
        lock.unlock();
        // persist(); FIXME
        co_return reply;
    }

    if( !args.entries.empty() ) {

        auto isConflict = [&](){
            int64_t base_index = args.preLogIndex + 1;
            for( int i = 0; i < args.entries.size(); i++ ) {
                 auto index = transfer( base_index+i );
                 if( index < 0 ) {
                    return true;
                 }
                 auto entry = logs[index];
                 if( entry.term != args.entries[i].term ) {
                    return true;
                 }
            }
            return false;  
        };

        if( isConflict() ) {
            logs = std::vector<Entry>( logs.begin(), logs.begin() + index+1 );
            for( auto& it : args.entries ) {
                logs.push_back( it );
            }
        }
        else {
            // 
        }

    }
    else {
        // 
    }

 // 提交
    if( args.leaderCommit > commitIndex ) {
        commitIndex = args.leaderCommit;
        if( args.leaderCommit > logs.back().index ) {
            commitIndex = logs.back().index;
        }
        // signal FIXME
    }

    lock.unlock();
    // persist(); FIXME
    co_return reply;
}

// 并行发送日志给各个flower
void Raft::doAppendEntries( ) {
    for( int i = 0; i < peers.size(); i++ ) {
        if( i == me ) continue;

        int wantSendIndex = nextIndex[i] - 1;
        if( wantSendIndex < logs.begin()->index ) {
            // 如果不需要复制日志，那就执行一次落盘
            // doInstallSnapShot( i ).start( [](auto&&){} ); // FIXME
        }
        else {
            // 否则，follower复制日志
            appendTo( i ).start( [](auto&&){} );
        }
    }
    // FIXME
    syncAwait( co_sleep( APD_WAIT_TIME ) );
}


void Raft::leaderInit() {
    ELOG_DEBUG << me << " server 当前状态:"<<state<<" currentterm = "<< currentTerm<<" 开始leader初始化";
    nextIndex = std::vector<int>( peers.size() ); // 需要发送给followers的下一条日志索引
    matchIndex = std::vector<int>( peers.size() ); // 每个follower的已经复制完毕的最高日志索引

    for( int i = 0; i < peers.size(); i++ ) {
        // FIXME
        nextIndex[i] = logs.back().index + 1; // log 的最后一个index + 1， 如果有snapshot的话，log可能就不是vector了
        matchIndex[i] = 0;
    }
    resetHeartBeatTime();
}


// 用于转换节点状态
void Raft::turnTo( STATE server_state ) {

    switch ( server_state ) {
        case Leader:
            ELOG_DEBUG << this->me << " server 在term "<< currentTerm<< "由状态"<< state << " 成为Leader"; 
            state = Leader;
            leaderInit(); // 做成为leader后的一系列初始化
            doAppendEntries(); // 成为leader后， 需要立刻给所有server发送心跳包，宣誓主权
            break;
        case Candidate:
            ELOG_DEBUG << this->me << " server 在term "<< currentTerm<< "由状态"<< state << "成为 Candidate";
            currentTerm++; // 当前term+1 （ +1后也未必是最新的，也许是没有最新日志的候选者，该候选者也需要保证不会得到选票 ）
            votedFor = me; // 候选者肯定需要选自己
            // persist();  // 持久化, 为什么一变成候选者就需要调一次落盘呢？ FIXME
            state = Candidate;
            break;
        case Follower:
            ELOG_DEBUG << this->me << " server 在term "<< currentTerm<< "由状态"<< state <<" 成为 Follower";
            state = Follower;
            break;
        default:
            ELOG_CRITICAL << " error state! ";
            exit(1);
    }

}

// 随机 150 ～300 ms 的超时时间
void Raft::resetElectionTime() {
    // 使用随机设备生成种子
    std::random_device rd;
    std::mt19937 gen(rd());
    // 定义随机数分布范围
    std::uniform_int_distribution<> dis(0, ELECTION_RANGE_TIME);
    // 生成随机数
    int random_number = dis(gen);
    int time_sleep = ELECTION_BASE_TIME + random_number;
    this->electionTime = Now() + time_sleep;
} 

void Raft::resetHeartBeatTime() {
    this->heartBeatTime = Now() + HEARTBEAT_TIME;
}

// ticker 中调用，选举期间全程加锁（ticker完成）
void Raft::doElection() {

    ELOG_DEBUG<< me <<" server 开始选举";

    int64_t voted_count = 1; // 首先给自己投一票
    Entry entry = logs.back(); // 最后一个log entry

    RequestVoteArgs args;
    args.term = currentTerm;
    args.candidateId = me;
    args.lastLogIndex = entry.index;
    args.lastLogTerm = entry.term;

    
    // 并行去对所有server做vote请求，纯异步任务，不等待, 不能将整个选举逻辑放在lambda里，无法获取锁
    auto vote = [&]( int id, RequestVoteReply& reply ) -> Lazy<void> {
        // RequestVoteReply reply{};
        // auto tmp_me = me; // for debug
        // ELOG_DEBUG<<id<<"被访问 raft 当前状态:me = "<<me<<"; state = "<<state<<" currentTerm = "<<currentTerm<<" votedFor = "<<votedFor;
        ELOG_DEBUG << me << " server 发送args, 候选人id:"<<args.candidateId<<" term:"<<args.term;
        auto ok = Raft::sendRequestVote(id, args, reply);
        // ELOG_DEBUG<<id<<" 被访问完，raft 当前状态:me = "<<me<<"; state = "<<state<<" currentTerm = "<<currentTerm<<" votedFor = "<<votedFor;
        
        if( !ok) {
            ELOG_DEBUG << me << " server 未收到 id:" << id <<" 的回复";
            co_return; 
        }
        ELOG_DEBUG << me << " server 收到reply, votefgranted:term = "<<reply.voteGranted<<" : "<< reply.term;
    };

    ELOG_DEBUG << me << " server 并行向所有节点发送选举请求";
    std::vector<RequestVoteReply> replys( peers.size() );
    for( int i = 0; i < peers.size(); i++ ) {
        if( i == me ) continue; // 自己不用
        // 异步去做投票请求，但是不会等着
        vote(i, replys[i] ).start( [&](auto&&){} );
    }

    // 等一会儿 replys
    // syncAwait( co_sleep( REQ_WAIT_TIME ) );

    ELOG_DEBUG<<me<<" server 得票情况:";
    for( auto& reply : replys ) {
        ELOG_DEBUG << reply.term<<" : "<<reply.voteGranted;
    }


    for( auto& reply : replys ) {
        if( currentTerm != reply.term || state != Candidate ) {
            // 选举超时，重新选举，无效回复
            ELOG_DEBUG << "无效reply";
            continue;
        }
        // term都没有其他server大，不是合格的候选者，身份转变为follower
        if( reply.term > currentTerm ) {
            ELOG_DEBUG << "reply的term更大，server回到follower状态";
            currentTerm = reply.term;
            votedFor = -1;
            // persist(); // FIXME
            turnTo( Follower );
            continue;
        }

        if( reply.voteGranted ) {
            voted_count++;
            ELOG_DEBUG<< me<<" server 获得一票, 当前票数:"<< voted_count;
            if( voted_count > peers.size() / 2 && state == Candidate ) {
                ELOG_DEBUG<< me<<" server 成为leader";
                turnTo( Leader ); 
                // 成为leader， 直接返回
                break;
            }
        }
    }
    


    // ELOG_DEBUG << me <<" server 离开doElection";

}


// 非leader， 一段时间没有收到来自leader的心跳包
// 就会默认leader挂了，于是开启下一轮（term）的选举（自己成为候选人）
Lazy<void> Raft::ticker() {
    ELOG_DEBUG<<"raft 初始状态:me = "<<me<<"; state = "<<state<<" currentTerm = "<<currentTerm<<" votedFor = "<<votedFor;
    // 重置选举超时时间
    resetElectionTime();

    ELOG_DEBUG << this->me << " server start ticker";

    while ( this->is_alive ) {

        // 原子执行
        co_await this->lock.coLock(); 

        switch ( this->state ) {
            ELOG_DEBUG << this->me << " server 状态"<< this->state;

            case Leader:  
                  
                if( heartBeatTimeOut() ) {
                    ELOG_DEBUG << this->me << " server 状态 Leader, 向所有follower发送日志";  
                    doAppendEntries(); // 如果超过需要发送心跳包的时间，发送心跳包
                    resetHeartBeatTime();
                }
                break;

            case Candidate:
                if( electionTimeOut() ) {
                    ELOG_DEBUG << this->me << " server 状态 Candidate, 继续发起选举"; 
                    // 本轮没有产生新的leader，重新选举（ 或者其他异常情况导致的一直为候选者状态 ）
                    turnTo( Candidate );
                    doElection(); // 开始选举流程
                    resetElectionTime(); // 刷新选举时间
                }
                break;
            case Follower:
                // 开启选举逻辑
                
                if( electionTimeOut() ) {
                    ELOG_DEBUG << this->me << " server 状态 Follower, 选举定时器超时，发起选举"; 
                    turnTo( Candidate );
                    doElection(); // 开始选举流程
                    resetElectionTime(); // 刷新选举时间
                }
                break;
            default:
                ELOG_CRITICAL << " error state! ";
                lock.unlock();
                exit(1);
        }
        lock.unlock();
 
        co_await co_sleep( GAP_TIME ); // 每次只睡一小会儿     
    }
}

void Raft::toCommit() {} // FIXME

//
// restore previously persisted state.
//
// func (rf *Raft) readPersist(data []byte) {
// 	if data == nil || len(data) < 1 { // bootstrap without any state?
// 		return
// 	}
// 	// Your code here (2C).
// 	// Example:
// 	// r := bytes.NewBuffer(data)
// 	// d := labgob.NewDecoder(r)
// 	// var xxx
// 	// var yyy
// 	// if d.Decode(&xxx) != nil ||
// 	//    d.Decode(&yyy) != nil {
// 	//   error...
// 	// } else {
// 	//   rf.xxx = xxx
// 	//   rf.yyy = yyy
// 	// }
// }

// 用于判断候选者的日志是否比投票者新
bool Raft::isUpToDate( int lastLogIndex, int lastLogTerm ) {
    Entry entry = logs.back();
    int index = entry.index;
    int term = entry.term;
    if( term == lastLogTerm ) {
        return lastLogIndex >= index;
    }
    return lastLogTerm > term;
}





Lazy<RequestVoteReply> Raft::RequestVote( RequestVoteArgs args ) {

    // for debug 先睡一段时间
    // co_await co_sleep( GAP_TIME*10 );

    RequestVoteReply reply{};
    co_await lock.coLock();
    // ELOG_DEBUG << me << " server 收到args, 参数为:"<<args.candidateId<<" "<<args.term;
    // 第一种情况：如果发投票请求的raft的term，都没有自己大，不可能投票给他,且把自己当前term返回，用于给它更新
    if( args.term < currentTerm ) { 
        reply.term = currentTerm;
        reply.voteGranted = false;
        ELOG_DEBUG << me << " server 的term " << currentTerm << " 高于 "<< args.candidateId<< " ,不投票 " ;
        goto REQ_RET; 
    }

    // 第二种，特殊情况，不管自己以前有没有投票，一旦收到更新term的投票，一定是转变为follower，参与该term的后续投票
    if( args.term > currentTerm ) {
        currentTerm = args.term;
        votedFor = -1;
        turnTo( Follower );
    }

    // 正常逻辑（args.term大于等于当前节点，且未投票或者投过该候选者的票，则将票投给该候选者）
    if( votedFor == -1 || votedFor == args.candidateId ) {
        
        // 增加安全性检查，如果投票者比候选者日志更 “新”， 拒绝投票
        if( !isUpToDate(args.lastLogIndex, args.lastLogTerm) ) {
            reply.voteGranted = false;
            reply.term = currentTerm;
            ELOG_DEBUG << me << " server 的日志比 " << args.candidateId <<  " 新 , 不投票 " ;
            goto REQ_RET;
        }


        votedFor = args.candidateId;
        reply.voteGranted = true;
        reply.term = currentTerm;
        // 注意投完票后刷新一下选举时间
        ELOG_DEBUG << me << " server 投票给" << args.candidateId<<" 并刷新选举时间";
        resetElectionTime();
        goto REQ_RET;
    }

    // 否则就是拒绝投票
    ELOG_DEBUG << me << " server 已经投票给" << votedFor<<" 拒绝投票给" << args.candidateId;
    reply.voteGranted = false;
    reply.term  = currentTerm;



REQ_RET:
    lock.unlock(); 
    // persist(); FIXME
    co_return reply;
}



bool Raft::sendRequestVote(int id, RequestVoteArgs args, RequestVoteReply& reply) {
    coro_rpc_client client;
    // ELOG_DEBUG<<id<<"被访问 raft 当前状态:me = "<<me<<"; state = "<<state<<" currentTerm = "<<currentTerm<<" votedFor = "<<votedFor;
    syncAwait(client.connect( "localhost", this->peers[id], milli_duration(DURATION_TIME) ) );
    auto ok = syncAwait( client.call_for<&Raft::RequestVote>(milli_duration( DURATION_TIME ), args) ); 
    // ELOG_DEBUG<<id<<"访问完 raft 当前状态:me = "<<me<<"; state = "<<state<<" currentTerm = "<<currentTerm<<" votedFor = "<<votedFor;
        
    if( !ok  ) {
        return false;
    }
    reply = std::move( ok.value() );
    return true;
}


// 构造下标为me的Raft
void Make( const std::vector<Raft_Ptr>& rafts, int64_t raft_id, Persister *persister ) {
    
    // 每个server都有与其他server通信的client
    int nums = rafts.size();
    std::vector<std::string> peers( nums );
    for( int i = 0; i < nums; i++ ) {
        if( i == raft_id ) continue;
        peers[i] = rafts[i]->port ;
    }
    rafts[raft_id]->peers = std::move( peers );
    // rafts[me]->persister = persister;
    
    // initialize from state persisted before a crash
	// rf.readPersist(persister.ReadRaftState())

    // 异步调用ticker，进行超时选举流程
    rafts[raft_id]->ticker().start( [](auto&&){} );

    
    return ;
}

