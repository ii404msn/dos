#include "agent/agent_impl.h"
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <gflags/gflags.h>
#include "util.h"

DECLARE_string(master_port);
DECLARE_string(agent_endpoint);
DECLARE_string(ce_port);
DECLARE_int32(agent_heart_beat_interval);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::FATAL;

namespace dos {

AgentImpl::AgentImpl():thread_pool_(4),
  master_(NULL),
  rpc_client_(NULL),
  mutex_(),
  c_set_(NULL),
  engine_(NULL){
  rpc_client_ = new RpcClient();
  c_set_ = new ContainerSet();
}

AgentImpl::~AgentImpl(){}

void AgentImpl::Poll(RpcController* controller,
                     const PollAgentRequest* request,
                     PollAgentResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  const ContainerPodNameIdx& pod_name_idx = c_set_->get<p_name_tag>();
  ContainerPodNameIdx::const_iterator pod_name_it = pod_name_idx.begin();
  PodStatus* pod = NULL;
  std::string last_pod_name = "";
  for (; pod_name_it != pod_name_idx.end(); ++pod_name_it) {
    if (last_pod_name != pod_name_it->pod_name_) {
      pod = response->mutable_status()->add_pstatus();
      pod->set_name(pod_name_it->pod_name_);
      pod->set_state(kPodRunning);
      last_pod_name = pod_name_it->pod_name_;
    }
    ContainerStatus* status = pod->add_cstatus();
    status->CopyFrom(*pod_name_it->status_);
  }
  done->Run();
}

void AgentImpl::Run(RpcController* controller,
                    const RunPodRequest* request,
                    RunPodResponse* response,
                    Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  const ContainerPodNameIdx& pod_name_idx = c_set_->get<p_name_tag>();
  const ContainerNameIdx& c_name_idx = c_set_->get<c_name_tag>();
  ContainerPodNameIdx::const_iterator pod_name_it = pod_name_idx.find(request->pod_name());
  if (pod_name_it != pod_name_idx.end()) {
    LOG(WARNING, "pod name %s exists in agent", request->pod_name().c_str());
    response->set_status(kRpcNameExist);
    done->Run();
    return;
  }
  std::deque<std::string> avilable_name;
  for (int32_t offset = 0; offset < request->pod().containers_size(); ++offset) { 
    std::string c_name = boost::lexical_cast<std::string>(offset) + "_container." + request->pod_name();
    if (c_name_idx.find(c_name) != c_name_idx.end()) {
      LOG(WARNING, "container name %s exists in agent ", c_name.c_str());
      response->set_status(kRpcNameExist);
      done->Run();
      return;
    }
    avilable_name.push_back(c_name);
  }

  for (int32_t index = 0; index < request->pod().containers_size(); ++index) {
    std::string c_name = avilable_name.front();
    avilable_name.pop_front();
    LOG(INFO, "add container %s", c_name.c_str());
    ContainerIdx idx;
    idx.name_ = c_name;
    idx.pod_name_ = request->pod_name();
    idx.status_ = new ContainerStatus();
    idx.status_->set_name(c_name);
    idx.status_->set_state(kContainerPending);
    idx.status_->mutable_spec()->CopyFrom(request->pod().containers(index));
    idx.logs_ = new std::deque<PodLog>();
    c_set_->insert(idx);
    thread_pool_.AddTask(boost::bind(&AgentImpl::HandleRunContainer, this, c_name));
  }
  response->set_status(kRpcOk);
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
  std::string engine_addr = "127.0.0.1:" + FLAGS_ce_port;
  ok = rpc_client_->GetStub(engine_addr, &engine_);
  if (!ok) {
    LOG(WARNING, "fail to build engine stub");
    return false;
  }
  thread_pool_.AddTask(boost::bind(&AgentImpl::HeartBeat, this));
  return true;
}

void AgentImpl::HeartBeat() {
  ::baidu::common::MutexLock lock(&mutex_);
  HeartBeatRequest* request = new HeartBeatRequest();
  request->set_hostname(::baidu::common::util::GetLocalHostName());
  request->set_endpoint(FLAGS_agent_endpoint);
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

void AgentImpl::HandleRunContainer(const std::string& c_name) {
  ::baidu::common::MutexLock lock(&mutex_);
  const ContainerNameIdx& c_name_idx = c_set_->get<c_name_tag>();
  ContainerNameIdx::const_iterator c_name_it = c_name_idx.find(c_name);
  if (c_name_it == c_name_idx.end()) {
    LOG(WARNING, "container with name %s has been removed", c_name.c_str());
    return;
  }
  ContainerState current_state = c_name_it->status_->state();
  if (current_state == kContainerPending) {
    RunContainerRequest request;
    request.mutable_container()->CopyFrom(c_name_it->status_->spec());
    request.set_name(c_name);
    RunContainerResponse response;
    bool ok = rpc_client_->SendRequest(engine_, &Engine_Stub::RunContainer,
                                       &request, &response,
                                       5, 1);
    if (!ok) {
      LOG(WARNING, "fail to run container %s", c_name.c_str());
      c_name_it->status_->set_state(kContainerError);
    }else {
      LOG(WARNING, "run container %s successfully", c_name.c_str());
      c_name_it->status_->set_state(kContainerRunning);
    }
  } else { 
  }
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
