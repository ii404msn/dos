#include "agent/agent_impl.h"

#include <gflags/gflags.h>
#include "util.h"

DECLARE_string(master_port);
DECLARE_int32(agent_heart_beat_interval);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::FATAL;

namespace dos {

AgentImpl::AgentImpl():thread_pool_(4),
  master_(NULL),
  rpc_client_(NULL),
  mutex_(){
  rpc_client_ = new RpcClient();
}

AgentImpl::~AgentImpl(){}

bool AgentImpl::Start() {
  ::baidu::common::MutexLock lock(&mutex_);
  bool ok = rpc_client_->GetStub("127,0,0.1:" + FLAGS_master_port, master_);
  if (!ok) {
    LOG(WARNING, "fail to build master stub");
    return false;
  }
  return true;
}

void AgentImpl::HeartBeat() {
  ::baidu::common::MutexLock lock(&mutex_);
  HeartBeatRequest* request = new HeartBeatRequest();
  request->hostname(::baidu::common::util::GetLocalHostName());
  request->endpoint("127.0.0.1:111");
  HeartBeatResponse* response = new HeartBeatResponse();
  boost::function<void (const HeartBeatRequest*, HeartBeatResponse*, bool, int)> callback;
  callback = boost::bind(&AgentImpl::::HeartBeatCallback, this, _1, _2, _3, _4);
  rpc_client_->AsyncRequest(master_,
                            &Master_Stub::HeartBeat,
                            request,
                            response,
                            callback,
                            2, 0);
}

void AgentImpl::HeartBeatCallback(const HeartBeatRequest* request,
                                  HeartBeatResponse* response,
                                  bool failed, int errno) {
  delete request;
  delete response;
  if (failed) {
    LOG(WARNING, "fail to make heartbeat to master , errno %d", errno);
  }
  thread_pool_.DelayTask(FLAGS_agent_heart_beat_interval, 
      boost::bind(&AgentImpl::HeartBeat, this));
}


} //end of dos
