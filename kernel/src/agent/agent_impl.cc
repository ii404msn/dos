#include "agent/agent_impl.h"
#include <boost/bind.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <gflags/gflags.h>
#include "util.h"
#include "string_util.h"
#include "common/resource_util.h"

DECLARE_string(ins_servers);
DECLARE_string(my_ip);
DECLARE_string(dos_root_path);
DECLARE_string(master_endpoint);
DECLARE_string(agent_endpoint);
DECLARE_string(ce_port);
DECLARE_int32(agent_heart_beat_interval);
DECLARE_int32(agent_port_range_start);
DECLARE_int32(agent_sync_container_stat_interval);
DECLARE_int32(agent_port_range_end);
DECLARE_double(agent_memory_rate);
DECLARE_double(agent_cpu_rate);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::FATAL;
using ::baidu::common::DEBUG;

namespace dos {

AgentImpl::AgentImpl():thread_pool_(4),
  master_(NULL),
  rpc_client_(NULL),
  mutex_(),
  c_set_(NULL),
  engine_(NULL),
  resource_mgr_(NULL),
  ins_(NULL),
  ins_watcher_(NULL){
  rpc_client_ = new RpcClient();
  c_set_ = new ContainerSet();
  resource_mgr_ = new ResourceMgr();
  ins_ = new InsSDK(FLAGS_ins_servers);
 
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
  resource_mgr_->Stat(response->mutable_status()->mutable_resource());
  done->Run();
}

void AgentImpl::Run(RpcController* controller,
                    const RunPodRequest* request,
                    RunPodResponse* response,
                    Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  Resource pod_require;
  for (int32_t cindex = 0; cindex < request->pod().containers_size(); cindex ++) {
    bool plus_ok = ResourceUtil::Plus(request->pod().containers(cindex).requirement(), 
                                        &pod_require);
    if (!plus_ok) {
      LOG(WARNING, "fail to calc total requirement for pod %s", request->pod_name().c_str());
      response->set_status(kRpcError);
      done->Run();
      return;
    }
  }
  bool alloc_ok = resource_mgr_->Alloc(pod_require);
  if (!alloc_ok) {
    response->set_status(kRpcNoResource);
    done->Run();
    return;
  }
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
    const Container& spec = request->pod().containers(index);
    LOG(INFO, "add container %s with cpu %d memory %s restart strategy %s",
        c_name.c_str(),
        spec.requirement().cpu().limit(),
        ::baidu::common::HumanReadableString(spec.requirement().memory().limit()).c_str(),
        RestartStrategy_Name(spec.restart_strategy()).c_str());
    ContainerIdx idx;
    idx.name_ = c_name;
    idx.pod_name_ = request->pod_name();
    idx.status_ = new ContainerStatus();
    idx.status_->set_name(c_name);
    idx.status_->set_state(kContainerPending);
    idx.status_->mutable_spec()->CopyFrom(spec);
    idx.logs_ = new std::deque<PodLog>();
    c_set_->insert(idx);
    thread_pool_.AddTask(boost::bind(&AgentImpl::KeepContainer, this, c_name));
  }
  response->set_status(kRpcOk);
  done->Run();
}

bool AgentImpl::Start() {
  std::string master_key = FLAGS_dos_root_path + FLAGS_master_endpoint; 
  ins_watcher_ = new InsWatcher(master_key, ins_, boost::bind(&AgentImpl::HandleMasterChange, this, _1));
  bool watch_ok = ins_watcher_->Watch();
  if (!watch_ok) {
    LOG(WARNING, "fail to watch master endpoint");
    return false;
  }
  bool load_ok = resource_mgr_->LoadFromLocal(FLAGS_agent_cpu_rate,
                                              FLAGS_agent_memory_rate);
  if (!load_ok) {
    LOG(WARNING, "fail to load memory and cpu");
    return false;
  }
  bool init_port_ok = resource_mgr_->InitPort(FLAGS_agent_port_range_start, 
                                              FLAGS_agent_port_range_end);
  if (!init_port_ok) {
    LOG(WARNING, "fail to init port range[%d, %d]",
        FLAGS_agent_port_range_start,
        FLAGS_agent_port_range_end);
    return false;
  }
  std::string master_addr;
  ins_watcher_->GetValue(&master_addr);
  LOG(INFO, "connect to master %s", master_addr.c_str());
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
  hostname_ = ::baidu::common::util::GetLocalHostName();  
  thread_pool_.AddTask(boost::bind(&AgentImpl::HeartBeat, this));
  return true;
}

void AgentImpl::Delete(RpcController* controller,
                       const DeletePodRequest* request,
                       DeletePodResponse* response,
                       Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  LOG(INFO, "delete pod %s", request->name().c_str());
  const ContainerPodNameIdx& pod_name_idx = c_set_->get<p_name_tag>();
  ContainerPodNameIdx::const_iterator pod_name_it = pod_name_idx.find(request->name());
  if (pod_name_it == pod_name_idx.end()) {
    LOG(WARNING, "pod name %s does not exist on agent", request->name().c_str());
    response->set_status(kRpcNotFound);
    done->Run();
    return;
  }
  for (; pod_name_it != pod_name_idx.end(); ++pod_name_it) {
    if (pod_name_it->pod_name_ != request->name()) {
      break;
    }
    pod_name_it->status_->set_state(kContainerKilled);
  }
  response->set_status(kRpcOk);
  done->Run();
}

void AgentImpl::WaitContainer(const std::string& c_name) {
  ::baidu::common::MutexLock lock(&mutex_);
  ContainerNameIdx& c_name_idx = c_set_->get<c_name_tag>();
  ContainerNameIdx::iterator c_name_it = c_name_idx.find(c_name);
  if (c_name_it == c_name_idx.end()) {
    LOG(WARNING, "container with name %s has been removed", c_name.c_str());
    return;
  }
  ShowContainerRequest request;
  request.add_names(c_name);
  ShowContainerResponse response;
  // only response status equals kRpcNotFound 
  // dos agent will clean container
  bool ok = rpc_client_->SendRequest(engine_, &Engine_Stub::ShowContainer,
                                     &request, &response,
                                     5, 1);
  // delete container is pending
  if (!ok || response.status() != kRpcNotFound) {
    LOG(INFO, "wait container %s to be deleted from engine", c_name.c_str());
    thread_pool_.DelayTask(FLAGS_agent_sync_container_stat_interval,
                             boost::bind(&AgentImpl::WaitContainer, this, c_name));
  } else {
    // delete container successfully and clean container in dos agent
    delete c_name_it->status_;
    delete c_name_it->desc_;
    c_set_->erase(c_name);
  }
}

void AgentImpl::HeartBeat() {
  ::baidu::common::MutexLock lock(&mutex_);
  HeartBeatRequest* request = new HeartBeatRequest();
  request->set_hostname(FLAGS_my_ip);
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

void AgentImpl::KeepContainer(const std::string& c_name) {
  ::baidu::common::MutexLock lock(&mutex_);
  const ContainerNameIdx& c_name_idx = c_set_->get<c_name_tag>();
  ContainerNameIdx::const_iterator c_name_it = c_name_idx.find(c_name);
  if (c_name_it == c_name_idx.end()) {
    LOG(WARNING, "container with name %s has been removed", c_name.c_str());
    return;
  }
  ContainerState current_state = c_name_it->status_->state();
  if (current_state == kContainerPending) {
    bool run_ok = RunContainer(c_name_it->status_);
    if (!run_ok) {
      c_name_it->status_->set_state(kContainerError);
    }else {
      c_name_it->status_->set_state(kContainerRunning);
    }
  } else if (current_state == kContainerRunning
             || current_state == kContainerBooting
             || current_state == kContainerPulling) {
    SyncContainerStat(c_name_it->status_);
  } else if (current_state == kContainerKilled) {
    bool ok = KillContainer(c_name_it->status_);
    // if kill container successfully , go to wait container to be killed
    if (ok) {
      thread_pool_.DelayTask(FLAGS_agent_sync_container_stat_interval,
                             boost::bind(&AgentImpl::WaitContainer, this, c_name));
      return;
    }
    // retry to kill container
    // TODO count retry kill
  }
  if (c_name_it->status_->state() != kContainerError) {
    thread_pool_.DelayTask(FLAGS_agent_sync_container_stat_interval,
        boost::bind(&AgentImpl::KeepContainer, this, c_name));
  }
}

bool AgentImpl::KillContainer(const ContainerStatus* status) {
  mutex_.AssertHeld();
  DeleteContainerRequest request;
  request.set_name(status->name());
  DeleteContainerResponse response;
  bool ok = rpc_client_->SendRequest(engine_, &Engine_Stub::DeleteContainer,
                                     &request, &response,
                                     5, 1);
  if (!ok) {
    LOG(WARNING, "fail to send request to engine to delete container %s for rpc err", status->name().c_str());
    return false;
  }
  if (response.status() != kRpcOk) {
    LOG(WARNING, "fail to send request to engine to delete container %s for %s",
        status->name().c_str(),
        RpcStatus_Name(response.status()).c_str());
    return false;
  }
  return true;
}

bool AgentImpl::RunContainer(const ContainerStatus* status) {
  mutex_.AssertHeld();
  RunContainerRequest request;
  request.mutable_container()->CopyFrom(status->spec());
  request.set_name(status->name());
  RunContainerResponse response;
  bool ok = rpc_client_->SendRequest(engine_, &Engine_Stub::RunContainer,
                                     &request, &response,
                                     5, 1);
  if (!ok) {
    LOG(WARNING, "fail to send request to engine to run container %s for rpc err", status->name().c_str());
    return false;
  }
  if (response.status() != kRpcOk) {
    LOG(WARNING, "fail to send request to engine to run container %s for %s",
        status->name().c_str(),
        RpcStatus_Name(response.status()).c_str());
    return false;
  }
  LOG(DEBUG, "send request to engine to run container %s successfully",
      status->name().c_str());
  return true;
}

bool AgentImpl::SyncContainerStat(ContainerStatus* status) {
  mutex_.AssertHeld();
  ShowContainerRequest request;
  request.add_names(status->name());
  ShowContainerResponse response;
  bool ok = rpc_client_->SendRequest(engine_, &Engine_Stub::ShowContainer,
                                     &request, &response,
                                     5, 1);
  if (!ok) {
    LOG(WARNING, "fail to sync container %s stat from engine for rpc err",
        status->name().c_str());
    status->set_state(kContainerError);
    return true;
  }
  if (response.status() != kRpcOk) {
    LOG(WARNING, "fail to sync container %s stat from engine for %s",
        status->name().c_str(),
        RpcStatus_Name(response.status()).c_str());
    status->set_state(kContainerError);
    return true;
  }
  if (response.containers_size() == 0) {
    LOG(WARNING, "container %s does not exist in engine",
        status->name().c_str());
    status->set_state(kContainerError);
    return true;
  }
  const ContainerOverview& overview = response.containers(0);
  status->set_state(overview.state());
  status->set_start_time(overview.start_time());
  status->set_boot_time(overview.boot_time());
  LOG(DEBUG, "sync container %s successfully", status->name().c_str());
  return true;
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

void AgentImpl::HandleMasterChange(const std::string& master_endpoint) {
  ::baidu::common::MutexLock lock(&mutex_);
  if (master_ != NULL) {
    delete master_;
  }
  LOG(INFO, "master endpoint changed , connect to master %s", master_endpoint.c_str());
  bool ok = rpc_client_->GetStub(master_endpoint, &master_);
  if (!ok) {
    LOG(WARNING, "fail to build master stub");
  }
}

} //end of dos
