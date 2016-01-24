#include "agent/agent_impl.h"
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
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

void AgentImpl::Poll(RpcController* controller,
                     const PollAgentRequest* request,
                     PollAgentResponse* response,
                     Closure* done) {
  done->Run();
}

void AgentImpl::Run(RpcController* controller,
                    const RunPodRequest* request,
                    RunPodResponse* response,
                    Closure* done) {
  done->Run();
}

bool AgentImpl::Start() {
  ::baidu::common::MutexLock lock(&mutex_);
  std::string master_addr = "127.0.0.1:" + FLAGS_master_port;
  bool ok = rpc_client_->GetStub(master_addr, &master_);
  if (!ok) {
    LOG(WARNING, "fail to build master stub");
    return false;
  }
  thread_pool_.AddTask(boost::bind(&AgentImpl::HeartBeat, this));
  return true;
}

void AgentImpl::HeartBeat() {
  ::baidu::common::MutexLock lock(&mutex_);
  HeartBeatRequest* request = new HeartBeatRequest();
  request->set_hostname(::baidu::common::util::GetLocalHostName());
  request->set_endpoint("127.0.0.1:111");
  HeartBeatResponse* response = new HeartBeatResponse();
  boost::function<void (const HeartBeatRequest*, HeartBeatResponse*, bool, int)> callback;
  callback = boost::bind(&AgentImpl::HeartBeatCallback, this, _1, _2, _3, _4);
  rpc_client_->AsyncRequest(master_,
                            &Master_Stub::HeartBeat,
                            request,
                            response,
                            callback,
                            2, 0);
}

void AgentImpl::HeartBeatCallback(const HeartBeatRequest* request,
                                  HeartBeatResponse* response,
                                  bool failed, int) { 
  if (failed) {
    LOG(WARNING, "fail to make heartbeat to master");
  }
  thread_pool_.DelayTask(FLAGS_agent_heart_beat_interval, 
      boost::bind(&AgentImpl::HeartBeat, this));
  delete request;
  delete response;
}

} //end of dos
