#ifndef KERNEL_AGENT_IMPL_H
#define KERNEL_AGENT_IMPL_H
#include "proto/agent.pb.h"

#include "thread_pool.h"
#include "proto/master.pb.h"
#include "rpc/rpc_client.h"
#include "mutex.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

class AgentImpl : public Agent{

public:
  AgentImpl();
  ~AgentImpl();
  void Query(RpcController* controller,
             const QueryAgentRequest* request,
             QueryAgentResponse* response,
             Closure* done);
  bool Start();
private:
  void HeartBeat();
  void HeartBeatCallback(const HeartBeatRequest* request,
                         HeartBeatResponse* response,
                         bool failed, int);

private:
  ::baidu::common::ThreadPool thread_pool_;
  Master_Stub* master_;
  RpcClient* rpc_client_;
  ::baidu::common::Mutex mutex_;
};

}// end of dos
#endif
