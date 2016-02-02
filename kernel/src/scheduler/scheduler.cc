#include "scheduler/scheduler.h"

#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

Scheduler::Scheduler(const std::string& master_addr):rpc_client_(NULL),
  master_(NULL), master_addr_(master_addr), pool_(5),
  mutex_(), agents_(NULL) {
  rpc_client_ = new RpcClient();
  agents_ = new boost::unordered_map<std::string, AgentOverview*>();
}

Scheduler::~Scheduler() {}

bool Scheduler::Start() {
  bool get_ok = rpc_client_->GetStub(master_addr_, &master_);
  if (!get_ok) {
    return false;
  }
  SyncAgentInfo();
  pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
  return true;
}

void Scheduler::SyncAgentInfo() {
  ::baidu::common::MutexLock lock(&mutex_);
  SyncAgentInfoRequest request;
  boost::unordered_map<std::string, AgentOverview*>::iterator agent_it = agents_->begin();
  for (; agent_it != agents_->end(); ++agent_it) {
    AgentVersion* version = request.add_versions();
    version->set_endpoint(agent_it->second->endpoint());
    version->set_version(agent_it->second->version());
  }
  SyncAgentInfoResponse response;
  bool ok = rpc_client_->SendRequest(master_, &Master_Stub::SyncAgentInfo,
                           &request, &response, 5, 1);
  if (!ok || response.status() != kRpcOk) {
    LOG(WARNING, "fail to sync agent info from master %s", master_addr_.c_str());
  } else {
    for (int32_t index = 0; index < response.diff_del_size(); ++index) {
      agent_it = agents_->find(response.diff_del(index));
      if (agent_it == agents_->end()) {
        continue;
      }
      LOG(INFO, "delete agent %s from scheduler", response.diff_del(index).c_str());
      // free agent overview
      delete agent_it->second;
      agents_->erase(agent_it);
    }

    for (int32_t mod_index = 0; mod_index < response.diff_mod_size(); ++mod_index) {
      const AgentOverview& new_agent = response.diff_mod(mod_index);
      agent_it = agents_->find(new_agent.endpoint());
      if (agent_it == agents_->end()) {
        LOG(INFO, "newly added agent %s", new_agent.endpoint().c_str());
        AgentOverview* copied_agent = new AgentOverview();
        copied_agent->CopyFrom(new_agent);
        agents_->insert(std::make_pair(new_agent.endpoint(), copied_agent));
      } else {
        LOG(INFO, "update agent %s", new_agent.endpoint().c_str());
        agent_it->second->CopyFrom(new_agent);
      }

    }
  }
  pool_.DelayTask(2000, boost::bind(&Scheduler::SyncAgentInfo, this));
}

void Scheduler::GetScaleUpPods() {
  LOG(INFO, "get scale up pods from %s", master_addr_.c_str());
  GetScaleUpPodRequest request;
  GetScaleUpPodResponse response;
  bool ok = rpc_client_->SendRequest(master_, &Master_Stub::GetScaleUpPod,
                           &request, &response, 5, 1);
  if (!ok || response.status() != kRpcOk) {
    LOG(WARNING, "fail to get scale up pods");
    pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
  } else {
    for (int i = 0; i < response.pods_size(); i++) {
      const PodOverview& pod = response.pods(i);
      LOG(INFO, "get scale up pod %s", pod.name().c_str());
    }
  }
  pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
}

} // namespace dos
