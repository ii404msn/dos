#include "master/node_manager.h"

#include <map>
#include <gflags/gflags.h>
#include "logging.h"
#include "timer.h"

DECLARE_string(nexus_servers);
DECLARE_string(master_root_path);
DECLARE_string(master_node_path_prefix);
DECLARE_int32(agent_heart_beat_timeout);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

NodeManager::NodeManager(FixedBlockingQueue<NodeStatus*>* node_status_queue,
                         FixedBlockingQueue<PodOperation*>* pod_opqueue):mutex_(),
  nodes_(NULL),
  nexus_(NULL),
  node_metas_(NULL),
  agent_conns_(NULL),
  thread_pool_(NULL),
  node_status_queue_(node_status_queue),
  pod_opqueue_(pod_opqueue),
  rpc_client_(NULL){
  nodes_ = new NodeSet();
  nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
  node_metas_ = new boost::unordered_map<std::string, NodeMeta*>();
  agent_conns_ = new boost::unordered_map<std::string, Agent_Stub*>();
  thread_pool_ = new ::baidu::common::ThreadPool(4);
  rpc_client_ = new RpcClient();
}

NodeManager::~NodeManager() {
  delete nodes_;
  delete nexus_;
  delete node_metas_;
}

bool NodeManager::Start() {
  ::baidu::common::MutexLock lock(&mutex_);
  //bool load_ok = LoadNodeMeta();
  ScheduleNextPoll();
  thread_pool_->AddTask(boost::bind(&NodeManager::WatchPodOpQueue, this));
  return true;
}

bool NodeManager::LoadNodeMeta() {
  mutex_.AssertHeld();
  std::string start_key = FLAGS_master_root_path + FLAGS_master_node_path_prefix + "/";
  std::string end_key = start_key + "~";
  ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
  while (!result->Done()) {
    assert(result->Error() == ::galaxy::ins::sdk::kOK);
    std::string key = result->Key();
    std::string node_raw_data = result->Value();
    NodeMeta* meta = new NodeMeta();
    bool ok = meta->ParseFromString(node_raw_data);
    if (!ok) {
      LOG(WARNING, "fail to load node with key %s", key.c_str());
      assert(0);
    }
    node_metas_->insert(std::make_pair(meta->hostname(), meta));
    result->Next();
  }
  LOG(INFO, "load node meta with count %u", node_metas_->size());
  return true;
}


void NodeManager::KeepAlive(const std::string& hostname,
                            const std::string& endpoint) {
  MutexLock lock(&mutex_);
  const NodeEndpointIndex& endpoint_idx = nodes_->get<endpoint_tag>();
  NodeEndpointIndex::const_iterator endpoint_it = endpoint_idx.find(endpoint);
  if (endpoint_it == endpoint_idx.end()) {
    LOG(INFO, "new node %s heart beat with endpoint %s", hostname.c_str(), endpoint.c_str());
    NodeIndex index;
    index.hostname_ = hostname;
    index.endpoint_ = endpoint; 
    index.status_ = new NodeStatus();
    boost::unordered_map<std::string, NodeMeta*>::iterator node_it = node_metas_->find(hostname);
    if (node_it == node_metas_->end()) {
      LOG(WARNING, "node %s has no meta in master", hostname.c_str());
    }else {
      index.status_->mutable_meta()->CopyFrom(*node_it->second);
    }
    int64_t task_id = thread_pool_->DelayTask(FLAGS_agent_heart_beat_timeout, boost::bind(&NodeManager::HandleNodeTimeout,
                                              this, endpoint));
    index.status_->set_task_id(task_id);
    nodes_->insert(index);
    return;
  }else {
    thread_pool_->CancelTask(endpoint_it->status_->task_id());
    int64_t task_id = thread_pool_->DelayTask(FLAGS_agent_heart_beat_timeout,
                                              boost::bind(&NodeManager::HandleNodeTimeout,
                                              this, endpoint));
    endpoint_it->status_->set_task_id(task_id);
  }
}

void NodeManager::PollNode(const std::string& endpoint) {
  mutex_.AssertHeld();
  boost::unordered_map<std::string, Agent_Stub*>::iterator agent_it = 
    agent_conns_->find(endpoint);
  if (agent_it == agent_conns_->end()) {
    Agent_Stub* agent = NULL;
    bool get_stub_ok = rpc_client_->GetStub(endpoint, &agent);
    if (!get_stub_ok) {
      LOG(WARNING, "fail to make a rpc connection with agent %s", endpoint.c_str());
      return;
    }
    agent_conns_->insert(std::make_pair(endpoint, agent));
    agent_it = agent_conns_->find(endpoint);
  }
  Agent_Stub* agent = agent_it->second;
  PollAgentRequest* request = new PollAgentRequest();
  PollAgentResponse* response = new PollAgentResponse();
  boost::function<void (const PollAgentRequest*, PollAgentResponse*, bool, int)> callback;
  callback = boost::bind(&NodeManager::PollNodeCallback, this, endpoint, _1, _2, _3, _4);
  rpc_client_->AsyncRequest(agent, &Agent_Stub::Poll,
                            request, response,
                            callback, 5, 1);
}

void NodeManager::PollNodeCallback(const std::string& endpoint,
    const PollAgentRequest* request, PollAgentResponse* response,
    bool failed, int) {
  for (int32_t index = 0; index < response->status().pstatus_size(); ++index) {
    const PodStatus& pod = response->status().pstatus(index);
    LOG(INFO, "pod %s state %s from agent %s", pod.name().c_str(), 
        PodState_Name(pod.state()).c_str(),
        endpoint.c_str());
  }
  delete request;
  delete response;
  ::baidu::common::MutexLock lock(&mutex_);
  agent_under_polling_.erase(endpoint);
  ScheduleNextPoll();
}

void NodeManager::SyncAgentInfo(const AgentVersionList& versions,
                                AgentOverviewList* agents,
                                StringList* del_list) {
  ::baidu::common::MutexLock lock(&mutex_);
  boost::unordered_map<std::string, int32_t> version_dict;
  for (int index = 0; index < versions.size(); ++index) {
    version_dict.insert(std::make_pair(versions.Get(index).endpoint(),
                                       versions.Get(index).version()));
  }
  const NodeEndpointIndex& endpoint_idx = nodes_->get<endpoint_tag>();
  NodeEndpointIndex::const_iterator endpoint_it = endpoint_idx.begin();
  for (; endpoint_it != endpoint_idx.end(); ++ endpoint_it) {
    boost::unordered_map<std::string, int32_t>::iterator version_it = 
      version_dict.find(endpoint_it->endpoint_);
    if (version_it != version_dict.end()) {
      // delete agent that has not been kNodeNormal
      if (endpoint_it->status_->state() != kNodeNormal) {
        del_list->Add()->assign(endpoint_it->endpoint_);
        continue;
      }
      // agent info has been updated
      if (endpoint_it->status_->version() != version_it->second) {
        AgentOverview* agent_overview = agents->Add();
        agent_overview->set_endpoint(endpoint_it->endpoint_);
        agent_overview->set_version(endpoint_it->status_->version());
        agent_overview->set_pod_count(endpoint_it->status_->pstatus_size());
        agent_overview->mutable_resource()->CopyFrom(endpoint_it->status_->resource());
        continue;
      }
    } else {
      if (endpoint_it->status_->state() != kNodeNormal) {
        continue;
      }
      AgentOverview* agent_overview = agents->Add();
      agent_overview->set_endpoint(endpoint_it->endpoint_);
      agent_overview->set_version(endpoint_it->status_->version());
      agent_overview->set_pod_count(endpoint_it->status_->pstatus_size());
      agent_overview->mutable_resource()->CopyFrom(endpoint_it->status_->resource());
    }
  }
}

void NodeManager::HandleNodeTimeout(const std::string& endpoint) {
  LOG(WARNING, "agent with endpoint %s timeout ", endpoint.c_str());
}

void NodeManager::WatchPodOpQueue() {
  PodOperation* pod_op = pod_opqueue_->Pop();
  switch(pod_op->type_) {
    case kKillPod:
      //
      break;
    case kRunPod:
      RunPod(pod_op->pod_->name(), 
             pod_op->pod_->endpoint(),
             pod_op->pod_->desc());
      break;
    default:
      LOG(WARNING, "no handle for pod");
  }
  delete pod_op;
  thread_pool_->AddTask(boost::bind(&NodeManager::WatchPodOpQueue, this));
}

void NodeManager::RunPod(const std::string& pod_name,
                         const std::string& endpoint,
                         const PodSpec& desc) {
  ::baidu::common::MutexLock lock(&mutex_);
  LOG(INFO, "run pod %s on agent %s", pod_name.c_str(), endpoint.c_str());
  Agent_Stub* agent_stub = NULL;
  boost::unordered_map<std::string, Agent_Stub*>::iterator agent_it = agent_conns_->find(endpoint);
  if (agent_it ==  agent_conns_->end()) {
    bool ok = rpc_client_->GetStub(endpoint, &agent_stub);
    if (ok) {
      LOG(WARNING, "fail to get agent stub witn endpoint %s for pod %s",
          endpoint.c_str(), pod_name.c_str());
      return;
    }
    agent_conns_->insert(std::make_pair(endpoint, agent_stub));
  } else {
    agent_stub = agent_it->second;
  }
  RunPodRequest* request = new RunPodRequest();
  request->set_pod_name(pod_name);
  request->mutable_pod()->CopyFrom(desc);
  RunPodResponse* response = new RunPodResponse();
  boost::function<void (const RunPodRequest*, RunPodResponse*, bool, int)> call_back;
  call_back = boost::bind(&NodeManager::RunPodCallback, this, _1, _2, _3, _4);
  rpc_client_->AsyncRequest(agent_stub, &Agent_Stub::Run,
                           request, response, call_back,
                           5, 0);
}

void NodeManager::ScheduleNextPoll() {
  mutex_.AssertHeld();
  if (agent_under_polling_.size() != 0) {
    return;
  }
  thread_pool_->DelayTask(5000, boost::bind(&NodeManager::StartPoll, this));
}

void NodeManager::StartPoll() {
  ::baidu::common::MutexLock lock(&mutex_);
  if (agent_under_polling_.size() != 0) {
    return;
  }
  LOG(INFO, "start the next poll turn");
  const NodeEndpointIndex& endpoint_idx = nodes_->get<endpoint_tag>();
  NodeEndpointIndex::const_iterator endpoint_it = endpoint_idx.begin();
  for (; endpoint_it != endpoint_idx.end(); ++endpoint_it) {
    // mark agent is under polling
    agent_under_polling_.insert(endpoint_it->endpoint_);
    PollNode(endpoint_it->endpoint_);
  }
  ScheduleNextPoll();
}

void NodeManager::RunPodCallback(const RunPodRequest* request,
                                RunPodResponse* response,
                                bool failed, int) {
  if (failed || response->status() != kRpcOk) {
    LOG(WARNING, "run pod %s fails", request->pod_name().c_str());
  } else {
    LOG(INFO, "run pod %s successfully", request->pod_name().c_str());
  }
  delete request;
  delete response;
}

} // end of dos
