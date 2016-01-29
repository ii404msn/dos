#include "master/pod_manager.h"

#include <boost/lexical_cast.hpp>
#include "logging.h"
#include "timer.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

PodManager::PodManager(FixedBlockingQueue<PodOperation*>* pod_opqueue,
                       FixedBlockingQueue<JobOperation*>* job_opqueue):pods_(NULL),
  scale_up_pods_(NULL),
  scale_down_pods_(NULL),
  fsm_(NULL),
  state_to_stage_(),
  pod_opqueue_(pod_opqueue),
  job_opqueue_(job_opqueue),
  job_desc_(NULL),
  tpool_(4){
  pods_ = new PodSet();
  scale_up_pods_ = new std::set<std::string>();
  scale_down_pods_ = new std::set<std::string>();
  fsm_ = new PodFSM();
  job_desc_ = new std::map<std::string, JobDesc>();
}

PodManager::~PodManager(){}

void PodManager::Start() {
  WatchJobOp();
}

void PodManager::WatchJobOp() {
  JobOperation* job_op = job_opqueue_->Pop();
  bool ok = false;
  switch(job_op->type_) {
    case kJobNewAdd:
      ok = NewAdd(job_op->job_->name(), job_op->job_->user_name(),
                  job_op->job_->desc().pod(), job_op->job_->desc().replica());
      job_desc_->insert(std::make_pair(job_op->job_->name(), job_op->job_->desc()));
      break;
    case kJobRemove:
      break;
    case kJobUpdate:
      break;
    default:
      LOG(WARNING, "no handle for job %s op ", job_op->job_->name().c_str());
  }
  if (!ok) {
    LOG(WARNING, "handle process job %s operation fails ", 
        job_op->job_->name().c_str());
  }
  delete job_op;
  tpool_.AddTask(boost::bind(&PodManager::WatchJobOp, this));
}

void PodManager::HandleStageRunningChanged(const Event& e) {
  mutex_.AssertHeld();
  const PodNameIndex& name_index = pods_->get<name_tag>();
  const std::string pod_name = boost::get<0>(e);
  const PodSchedStage& to_stage = boost::get<2>(e);
  PodNameIndex::const_iterator name_it = name_index.find(pod_name);
  if (name_it == name_index.end()) {
    LOG(WARNING, "no pod with name %s in pod manager", pod_name.c_str());
    return;
  }
  // pod on agent goes to death stage, clean it and change stage to
  // death
  if (to_stage == kPodSchedStageDeath) {
    // send kill cmd to agent to clean this pod
    PodOperation* op = new PodOperation();
    op->pod_ = name_it->pod_;
    op->type_ = kKillPod;
    pod_opqueue_->Push(op);
    name_it->pod_->set_stage(kPodSchedStageDeath);
    LOG(INFO, "clean dead pod %s ", pod_name.c_str());
  } else if (to_stage == kPodSchedStageRunning) {
    // pod is running well
  } else if (to_stage == kPodSchedStagePending) {
    // pod is lost , reschedule it
    name_it->pod_->set_stage(kPodSchedStagePending);
    // reset pod state
    name_it->pod_->set_state(kPodPending);
    // record pending time
    name_it->pod_->set_start_pending_time(::baidu::common::timer::get_micros());
    scale_up_pods_->insert(pod_name);
    LOG(INFO, "put pod %s into pending queue again", pod_name.c_str());
  } else if (to_stage == kPodSchedStageRemoved) {
    // remove pod , clean it on agent and change stage to kPodSchedStageRemoved
    PodOperation* op = new PodOperation();
    op->pod_ = name_it->pod_;
    op->type_ = kKillPod;
    pod_opqueue_->Push(op);
    name_it->pod_->set_stage(kPodSchedStageRemoved);
    LOG(INFO, "kill running pod %s", pod_name.c_str());
  } else {
    LOG(WARNING, "[alert]invalidate incoming stage %s with pod %s",
        PodSchedStage_Name(to_stage).c_str(),
        pod_name.c_str());
  }
}

void PodManager::HandleStagePendingChanged(const Event& e) {
  mutex_.AssertHeld();
  const std::string pod_name = boost::get<0>(e);
  const PodSchedStage& to_stage = boost::get<2>(e);
  PodNameIndex& name_index = pods_->get<name_tag>();
  PodNameIndex::iterator name_it = name_index.find(pod_name);
  if (name_it == name_index.end()) {
    LOG(WARNING, "no pod with name %s in pod manager", pod_name.c_str());
    return;
  }
  // schedule pending pod 
  if (to_stage == kPodSchedStageRunning) {
    LOG(INFO, "schedule pod %s to agent %s", PodSchedStage_Name(to_stage).c_str(), 
        name_it->endpoint_.c_str());
    PodOperation* op = new PodOperation();
    op->pod_ = name_it->pod_;
    op->type_ = kRunPod;
    pod_opqueue_->Push(op);
    name_it->pod_->set_stage(kPodSchedStageRunning);
  } else if (to_stage == kPodSchedStageRemoved) {
    // remove pod with pending stage , just clean it
    LOG(INFO, "delete pod %s", pod_name.c_str());
    // earse from scale up queue
    scale_up_pods_->erase(pod_name);
    // free pod status
    delete name_it->pod_;
    name_index.erase(name_it);
  } else {
    LOG(WARNING, "[alert]invalidate incoming stage %s with pod %s",
        PodSchedStage_Name(to_stage).c_str(),
        pod_name.c_str()); 
  }
}

void PodManager::SchedPods(const std::vector<boost::tuple<std::string, std::string> >& pods) {
  ::baidu::common::MutexLock lock(&mutex_);
  PodNameIndex& name_index = pods_->get<name_tag>();
  std::vector<boost::tuple<std::string, std::string> >::const_iterator it = pods.begin();
  for (; it != pods.end(); ++it) {
    boost::tuple<std::string, std::string> pod = *it;
    const std::string pod_name = boost::get<1>(pod);
    const std::string endpoint = boost::get<0>(pod);
    PodNameIndex::iterator name_it = name_index.find(pod_name);
    if (name_it == name_index.end()) {
      LOG(WARNING, "pod with name %s does not exist", pod_name.c_str());
      continue;
    }
    if (name_it->pod_->stage() != kPodSchedStagePending) {
      LOG(WARNING, "pod with name %s has been scheduled", pod_name.c_str());
      continue;
    }
    PodIndex index = *name_it;
    index.endpoint_ = endpoint;
    index.pod_->set_endpoint(index.endpoint_);
    name_index.replace(name_it, index);
    DispatchEvent(boost::make_tuple(pod_name, kPodSchedStagePending, kPodSchedStageRunning));
  }
}

// add nonexist pod
bool PodManager::NewAdd(const std::string& job_name,
                        const std::string& user_name,
                        const PodSpec& desc,
                        int32_t replica) {
  ::baidu::common::MutexLock lock(&mutex_);
  const PodNameIndex& name_index = pods_->get<name_tag>();
  std::vector<std::string> avilable_name;
  for (int32_t offset = 0; offset < replica; ++offset) {
    std::string pod_name = boost::lexical_cast<std::string>(offset) + "." + job_name;
    PodNameIndex::const_iterator name_it = name_index.find(pod_name);
    if (name_it != name_index.end()) {
      LOG(WARNING, "fail alloc pod name %s for that it has been used", pod_name.c_str());
      return false;
    }
    avilable_name.push_back(pod_name);
  }
  std::vector<std::string>::iterator pod_name_it = avilable_name.begin();
  for (; pod_name_it != avilable_name.end(); ++pod_name_it) {
    PodStatus* pod = new PodStatus();
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
    scale_up_pods_->insert(pod->name());
    LOG(INFO, "add new pod %s", pod->name().c_str());
  }
  return true;
}

void PodManager::SyncPodsOnAgent(const std::string& endpoint,
                                 std::map<std::string, PodStatus>& pods) {
  ::baidu::common::MutexLock lock(&mutex_);
  const PodEndpointIndex& endpoint_index = pods_->get<endpoint_tag>();
  PodEndpointIndex::const_iterator endpoint_it = endpoint_index.find(endpoint);
  std::vector<Event> events;
  std::set<std::string> processed_pods;
  for (; endpoint_it != endpoint_index.end(); ++endpoint_it) {
    std::map<std::string, PodStatus>::iterator pod_it = pods.find(endpoint_it->name_);
    if (pod_it == pods.end()) {
      LOG(WARNING, "pod %s lost from agent %s", endpoint_it->name_.c_str(),
          endpoint.c_str());
      // change pods that are lost to kPodSchedStagePending
      events.push_back(boost::make_tuple(endpoint_it->name_, 
                                         endpoint_it->pod_->stage(),
                                         kPodSchedStagePending));
    } else {
      // TODO add more stat eg cpu, memory, disk io
      LOG(INFO, "pod %s from agent %s with stat: state %s", endpoint_it->name_.c_str(),
          endpoint.c_str(), PodState_Name(pod_it->second.state()).c_str());
      // update status on agent
      PodSchedStage to_stage = state_to_stage_[pod_it->second.state()];
      endpoint_it->pod_->mutable_cstatus()->CopyFrom(pod_it->second.cstatus());
      endpoint_it->pod_->set_state(pod_it->second.state());
      endpoint_it->pod_->set_boot_time(pod_it->second.boot_time());
      events.push_back(boost::make_tuple(endpoint_it->name_,
                                         endpoint_it->pod_->stage(),
                                         to_stage));
      // record the processed pods
      processed_pods.insert(endpoint_it->name_);
    }
  }

  for (size_t i = 0; i < events.size(); i++) {
    DispatchEvent(events[i]);
  }
  // TODO add handle expired pods

}

void PodManager::DispatchEvent(const Event& e) {
  mutex_.AssertHeld();
  const std::string pod_name = boost::get<0>(e);
  const PodSchedStage& current = boost::get<1>(e);
  const PodSchedStage& to_stage = boost::get<2>(e); 
  PodFSM::iterator it = fsm_->find(current);
  if (it == fsm_->end()) {
    LOG(WARNING, "no event handle for pod %s change stage from %s to %s",
        pod_name.c_str(), PodSchedStage_Name(current).c_str(), 
        PodSchedStage_Name(to_stage).c_str());
    return;
  }
  it->second(e);
}

}// namespace dos
