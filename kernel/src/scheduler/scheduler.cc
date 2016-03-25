#include "scheduler/scheduler.h"

#include <gflags/gflags.h>
#include "logging.h"
#include "string_util.h"
#include "timer.h"
#include "common/resource_util.h"

DECLARE_string(ins_servers);
DECLARE_string(dos_root_path);
DECLARE_string(master_endpoint);

DECLARE_int32(scheduler_sync_agent_info_interval);
DECLARE_int32(scheduler_feasibility_factor);
DECLARE_int32(scheduler_max_pod_count);
DECLARE_double(scheduler_score_longrun_pod_factor);
DECLARE_double(scheduler_score_pod_factor);
DECLARE_double(scheduler_score_cpu_factor);
DECLARE_double(scheduler_score_memory_factor);

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

static bool SchedCellDesc(const SchedCell& left, const SchedCell& right) {
  return left.priority > right.priority;
}

static bool AgentScoreAsc(const ProposeCell& left, const ProposeCell& right) {
  return left.score > right.score;
}

Scheduler::Scheduler():rpc_client_(NULL),
  master_(NULL), pool_(5),
  mutex_(), agents_(NULL),
  ins_(NULL), ins_watcher_(NULL){
  rpc_client_ = new RpcClient();
  agents_ = new boost::unordered_map<std::string, AgentOverview*>();
  ins_ = new InsSDK(FLAGS_ins_servers);
}

Scheduler::~Scheduler() {}

bool Scheduler::Start() {
  std::string master_key = FLAGS_dos_root_path + FLAGS_master_endpoint; 
  ins_watcher_ = new InsWatcher(master_key, ins_, boost::bind(&Scheduler::HandleMasterChange, this, _1));
  bool watch_ok = ins_watcher_->Watch();
  if (!watch_ok) {
    LOG(WARNING, "fail to watch master endpoint");
    return false;
  }
  ins_watcher_->GetValue(&master_addr_);
  LOG(INFO, "connect to master %s", master_addr_.c_str());
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
        AgentOverview* copied_agent = new AgentOverview();
        copied_agent->CopyFrom(new_agent);
        LOG(INFO, "newly added agent %s with resource cpu %ld mem %s",
            new_agent.endpoint().c_str(),
            new_agent.resource().cpu().limit(),
            ::baidu::common::HumanReadableString(new_agent.resource().memory().limit()).c_str());
        agents_->insert(std::make_pair(new_agent.endpoint(), copied_agent));
      } else {
        agent_it->second->CopyFrom(new_agent);
        LOG(INFO, "update agent %s with resource cpu total:%ld assigned:%ld  mem total:%s assigned:%s",
            new_agent.endpoint().c_str(),
            new_agent.resource().cpu().limit(),
            new_agent.resource().cpu().assigned(),
            ::baidu::common::HumanReadableString(new_agent.resource().memory().limit()).c_str(),
            ::baidu::common::HumanReadableString(new_agent.resource().memory().assigned()).c_str());
      }

    }
  }
  pool_.DelayTask(FLAGS_scheduler_sync_agent_info_interval, 
                  boost::bind(&Scheduler::SyncAgentInfo, this));
}

void Scheduler::GetScaleUpPods() {
  LOG(INFO, "get scale up pods from %s", master_addr_.c_str());
  GetScaleUpPodRequest request;
  GetScaleUpPodResponse response;
  bool rpc_ok = false;
  {
    ::baidu::common::MutexLock lock(&mutex_);
    rpc_ok = rpc_client_->SendRequest(master_, &Master_Stub::GetScaleUpPod,
                                     &request, &response, 5, 1);
  }
   if (!rpc_ok || response.status() != kRpcOk) {
    LOG(WARNING, "fail to get scale up pods");
    pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
  } else {
    std::map<std::string, SchedCell> cells;
    BuildScaleUpSchedCell(response, cells);
    std::vector<SchedCell> sorted_cells;
    SortSchedCell(cells, sorted_cells);
    ProcessScaleUpCell(sorted_cells);
  }
  pool_.DelayTask(1000, boost::bind(&Scheduler::GetScaleUpPods, this));
}

void Scheduler::BuildScaleUpSchedCell(const GetScaleUpPodResponse& response,
                                      std::map<std::string, SchedCell>& cells) {
  for (int32_t index = 0; index < response.pods_size(); ++index) {
    const PodOverview& pod = response.pods(index);
    SchedCell& cell = cells[pod.job_name()];
    cell.priority = pod.type();
    cell.type = pod.type();
    cell.job_name = pod.job_name();
    cell.pods.push_back(pod.name());
    cell.resource = pod.requirement();
    cell.action = ScaleUp;
    cell.feasibile_count = cell.pods.size() * FLAGS_scheduler_feasibility_factor;
  }
}

void Scheduler::SortSchedCell(const std::map<std::string, SchedCell>& cells,
                              std::vector<SchedCell>& sorted_cells) {
  std::map<std::string, SchedCell>::const_iterator cell_it = cells.begin();
  for (; cell_it != cells.end(); ++cell_it) {
    sorted_cells.push_back(cell_it->second);
  }
  std::sort(sorted_cells.begin(), sorted_cells.end(), SchedCellDesc);
}

void Scheduler::ProcessScaleUpCell(std::vector<SchedCell>& cells) {
  ::baidu::common::MutexLock lock(&mutex_);
  int64_t feasibile_check_start = ::baidu::common::timer::get_micros();
  std::vector<std::string> endpoints;
  boost::unordered_map<std::string, AgentOverview*>::iterator a_it = agents_->begin();
  for (; a_it != agents_->end(); ++a_it) {
    endpoints.push_back(a_it->first);
  }
  Shuffle(endpoints);
  std::vector<std::string>::iterator e_it = endpoints.begin();
  int32_t check_count = 0;
  for (; e_it !=  endpoints.end(); ++e_it) {
    a_it = agents_->find(*e_it);
    AgentOverview* agent = a_it->second;
    Resource total = agent->resource();
    std::vector<SchedCell>::iterator cell_it = cells.begin();
    bool all_fit = true;
    for (; cell_it != cells.end(); ++cell_it) {
      if (cell_it->agents.size() >= (size_t)cell_it->feasibile_count) {
        break;
      }
      check_count++;
      all_fit = false;
      bool alloc_ok = ResourceUtil::Alloc(cell_it->resource,
                                          &total);
      if (alloc_ok) {
        LOG(DEBUG, "agent %s fit pod of job %s resource requirement",
            a_it->second->endpoint().c_str(),
            cell_it->job_name.c_str());
        ProposeCell p_cell;
        p_cell.overview = *agent;
        cell_it->agents.push_back(p_cell);
      }
    }
    if (all_fit) {
      break;
    }
  }
  int64_t consumed = (::baidu::common::timer::get_micros() - feasibile_check_start)/1000;
  LOG(INFO, "checking feasibility consumes %ld ms with %d time calculation ",
      consumed, check_count);
  for (size_t cindex = 0; cindex < cells.size(); ++cindex) {
    if (cells[cindex].agents.size() <=0) {
      continue;
    }
    pool_.AddTask(boost::bind(&Scheduler::ProcessScaleUpPropose, this, cells[cindex]));
  }
}

void Scheduler::ProcessScaleUpPropose(SchedCell cell) {
  cell.Score();
  ScaleUpProposeRequest request;
  ScaleUpProposeResponse response;
  size_t agent_cursor = 0;
  for (size_t index = 0; index < cell.pods.size(); ++index) {
    if (agent_cursor >= cell.agents.size()){
      break;
    }
    Propose* propose = request.add_proposes();
    propose->set_pod_name(cell.pods[index]);
    propose->set_endpoint(cell.agents[agent_cursor].overview.endpoint());
    agent_cursor++;
  }
  if (agent_cursor >0) {
    bool ok = rpc_client_->SendRequest(master_, &Master_Stub::ScaleUpPropose,
                                      &request, &response, 5, 1);
    if (ok && response.status() == kRpcOk) {
      LOG(INFO, "propose job %s successfully", cell.job_name.c_str());
    }else { 
      LOG(WARNING, "fail to propose job %s ", cell.job_name.c_str());
    }
  }
}

void Scheduler::HandleMasterChange(const std::string& master_endpoint) {
  ::baidu::common::MutexLock lock(&mutex_);
  if (master_ != NULL) {
    delete master_;
  }
  master_addr_ = master_endpoint;
  LOG(INFO, "master endpoint changed , connect to master %s", master_endpoint.c_str());
  bool ok = rpc_client_->GetStub(master_endpoint, &master_);
  if (!ok) {
    LOG(WARNING, "fail to build master stub");
  }
}

void SchedCell::Score() {
  int64_t score_start = ::baidu::common::timer::get_micros();
  std::vector<ProposeCell>::iterator a_it = agents.begin();
  for (; a_it != agents.end(); ++a_it) {
    a_it->score = score_func_map.find(type)->second(a_it->overview);
    LOG(INFO, "score agent %s with score %f", a_it->overview.endpoint().c_str(),
        a_it->score);
  }
  std::sort(agents.begin(), agents.end(), AgentScoreAsc);
  int64_t consumed = (::baidu::common::timer::get_micros() - score_start)/1000;
  LOG(INFO, "scoring agent for job %s consumes %ld ms",
      job_name.c_str(), consumed);
}

double SchedCell::ScoreForLongrunPod(const AgentOverview& agent) {
  double cpu_load = agent.resource().cpu().assigned() * FLAGS_scheduler_score_cpu_factor/
    agent.resource().cpu().limit();
  double mem_load = agent.resource().memory().assigned() * FLAGS_scheduler_score_memory_factor/
    agent.resource().memory().limit();
  int32_t long_run_count = 0;
  for (int32_t index = 0; index < agent.pods_size(); ++index) {
    const PodOverview& pod = agent.pods(index);
    if (pod.type() == kPodLongrun
        || pod.type() == kPodSystem) {
      long_run_count++;
    }
  }
  double long_run_load = long_run_count * FLAGS_scheduler_score_longrun_pod_factor/
    FLAGS_scheduler_max_pod_count ;
  double pod_load = (agent.pods_size() - long_run_count) * FLAGS_scheduler_score_pod_factor /
    FLAGS_scheduler_max_pod_count;
  return exp(cpu_load) + exp(mem_load) + exp(long_run_load) + exp(pod_load);
}

double SchedCell::ScoreForBatchPod(const AgentOverview& agent) {
  double cpu_load = agent.resource().cpu().assigned() * FLAGS_scheduler_score_cpu_factor/
    agent.resource().cpu().limit();
  double mem_load = agent.resource().memory().assigned() * FLAGS_scheduler_score_memory_factor/
    agent.resource().memory().limit();
  double pod_load = agent.pods_size() * FLAGS_scheduler_score_pod_factor /
    FLAGS_scheduler_max_pod_count;
  return exp(cpu_load) + exp(mem_load) + exp(pod_load);
}

} // namespace dos
