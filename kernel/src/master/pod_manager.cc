#include "master/pod_manager.h"

#include <boost/lexical_cast.hpp>
#include "logging.h"
#include "timer.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

PodManager::PodManager(FixedBlockingQueue<PodOperation*> op_queue):pods_(NULL),
  scale_down_pods_(NULL),
  scale_down_pods_(NULL),
  fsm_(NULL),
  state_to_stage_(),
  op_queue_(op_queue_){
  pods_ = new PodSet();
  scale_up_pods_ = new std::set<std::string>();
  scale_down_pods_ = new std::set<std::string>();
  fms_ = new PodStage();
}

PodManager::~PodManager(){}

void PodManager::HandleStageRunningChanged(const Event& e) {
  mutex_.AssertHeld();
  const PodNameIndex& name_index = pods->get<name_tag>();
  PodNameIndex::const_iterator name_it = name_index.find(e[0].c_str());
  if (name_it == name_index.end()) {
    LOG(WARNING, "no pod with name %s in pod manager", e[0].c_str());
    return;
  }
  // pod on agent goes to death stage, clean it and change stage to
  // death
  if (e[2] == kPodSchedStageDeath) {
    // send kill cmd to agent to clean this pod
    PodOperation* op = new PodOperation();
    op->status = name_it->status;
    op->type = kKillPod;
    op_queue_->Push(op);
    name_it->status->set_stage(kPodSchedStageDeath);
    LOG(INFO, "clean dead pod %s ", e[0].c_str());
  } else if (e[2] == kPodSchedStageRunning) {
    // pod is running well
  } else if (e[2] == kPodSchedStagePending) {
    // pod is lost , reschedule it
    name_it->status->set_stage(kPodSchedStagePending);
    // reset pod state
    name_it->status->set_state(kPodPending);
    // record pending time
    name_it->status->set_start_pending_time(::baidu::common::timer::get_micros());
    scale_up_pods_->push_back(e[0]);
    LOG(INFO, "put pod %s into pending queue again", e[0].c_str());
  } else if (e[2] == kPodSchedStageRemoved) {
    // remove pod , clean it on agent and change stage to kPodSchedStageRemoved
    PodOperation* op = new PodOperation();
    op->status = name_it->status;
    op->type = kKillPod;
    op_queue_->Push(op);
    name_it->status->set_stage(kPodSchedStageRemoved);
    LOG(INFO, "kill running pod %s", e[0].c_str());
  } else {
    LOG(WARNING, "[alert]invalidate incoming stage %s with pod %s",
        PodSchedStage_Name(e[2]).c_str(),
        e[0].c_str());
  }
}

// add nonexist pod
bool PodManager::NewAdd(const std::string& job_name,
                        const std::string& user_name,
                        const PodSpec& desc,
                        int32_t replica) {
  ::baidu::common::MutexLock lock(&mutex_);
  const PodNameIndex& name_index = pods->get<name_tag>();
  std::vector<std::string> avilable_name;
  for (int32_t offset = 0; index < replica; ++offset) {
    std::string pod_name = boost::lexical_cast<std::string>  + "." + job_name;
    PodNameIndex::const_iterator name_it = name_index.find(pod_name);
    if (name_it != name_index.end()) {
      LOG(WARNING, "fail alloc pod name %s for that it has been used", pod_name.c_str());
      return false;
    }
    avilable_name.push_back(pod_name);
  }
  std::vector<std::string>::iterator pod_name_it = avilable_name.begin();
  for (; pod_name_it != avilable_name.end(); ++pod_name_it) {
    PodStatus pod = new PodStatus();
    pod->mutable_desc()->CopyFrom(desc);
    pod->set_name(*pod_name_it);
    pod->set_stage(kPodSchedStagePending);
    pod->set_state(kPodPending);
    pod->set_sched_time(::baidu::common::timer::get_micros());
    PodIndex pod_index;
    pod_index.name_ = pod->name();
    pod_index.user_name_ = user_name;
    pod_index.job_name_ = job_name;
    pod_index.pod_ = pod;
    pods_->insert(pod_index);
    scale_up_pods_->push_back(pod_name);
    LOG(INFO, "add new pod %s", pod->name().c_str());
  }
  return true;
}

bool PodManager::SyncPodsOnAgent(const std::string& endpoint,
                                 std::map<std::string, PodStatus>& pods) {
  ::baidu::common::MutexLock lock(&mutex_);
  const PodEndpointIndex& endpoint_index = pods_->get<endpoint_tag>();
  PodEndpointIndex::const_iterator endpoint_it = endpoint_index.find(endpoint);
  std::vector<Event> events;
  for (; endpoint_it != endpoint_index.end(); ++endpoint_it) {
    std::map<std::string, PodStatus>::iterator pod_it = pods.find(endpoint_it->name);
    if (pod_it == pods.end()) {
      LOG(WARNING, "pod %s lost from agent %s", endpoint_it->name.c_str(),
          endpoint.c_str());
      // change pods that are lost to kPodSchedStagePending
      events.push_back(boost::make_tuple(endpoint_it->name, 
                                         endpoint_it->status->stage(),
                                         kPodSchedStagePending));
    } else {
      // TODO add more stat eg cpu, memory, disk io
      LOG(INFO, "pod %s from agent %s with stat: state %s", endpoint_it->name.c_str(),
          endpoint.c_str(), PodState_Name(pod_it->second.state()).c_str());
      // update status on agent
      PodSchedStage to_stage = state_to_stage_[pod_it->second.state()];
      endpoint_it->status->mutable_cstatus()->CopyFrom(pod_it->second.cstatus());
      endpoint_it->status->set_state(pod_it->second.state());
      endpoint_it->status->set_boot_time(pod_it->second.boot_time());
      events.push_back(boost::make_tuple(endpoint_it->name,
                                         endpoint_it->status->stage(),
                                         to_stage))
      // erase pod that is handled, the left pods are expired
      pods.erase(pod_it);
    }
  }

  for (size_t i = 0; i < events.size(); i++) {
    DispatchEvent(events[i]);
  }
  // TODO add handle expired pods
}

void PodManager::DispatchEvent(const Event& e) {
  mutex_.AssertHeld();
  PodFSM::iterator it = fsm_->find(e[1]);
  if (it == fsm_->end()) {
    LOG(WARNING, "no event handle for pod %s change stage from %s to %s",
        e[0].c_str(), PodSchedStage_Name(e[1]),c_str(), PodSchedStage_Name(e[2]).c_str());
    return;
  }
  fsm_->second(e);
}

}// namespace dos
