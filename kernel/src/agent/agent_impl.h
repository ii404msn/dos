#ifndef KERNEL_AGENT_IMPL_H
#define KERNEL_AGENT_IMPL_H

#include "thread_pool.h"
#include "master/master.pb.h"
#include "rpc/rpc_client.h"
#include "mutex.h"

namespace dos {

class AgentImpl {

public:
  AgentImpl();
  ~AgentImpl();
  bool Start();
private:
  void HeartBeat();
  void HeartBeatCallback(const HeartBeatRequest* request,
                         HeartBeatResponse* response,
                         bool failed, int errno);
private:
  ::baidu::common::ThreadPool thread_pool_;
  Master_Stub* master_;
  RpcClient* rpc_client_;
  ::baidu::common::Mutex mutex_;
};

}// end of dos
#endif
