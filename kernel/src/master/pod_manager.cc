#include "master/pod_manager.h"

#include <boost/lexical_cast.hpp>
#include "common/resource_util.h"
#include "logging.h"
#include "timer.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

PodManager::PodManager(FixedBlockingQueue<PodOperation*>* pod_opqueue,
                       FixedBlockingQueue<JobOperation*>* job_opqueue,
                       FixedBlockingQueue<NodeStatus*>* node_opqueue):pods_(NULL),
  scale_up_jobs_(NULL),
  scale_down_jobs_(NULL),
  fsm_(NULL),
  state_to_stage_(),
  pod_opqueue_(pod_opqueue),
  job_opqueue_(job_opqueue),
  job_desc_(NULL),
  tpool_(4),
  node_opqueue_(node_opqueue){
  pods_ = new PodSet();
  scale_up_jobs_ = new std::set<std::string>();
  scale_down_jobs_ = new std::set<std::string>();
  fsm_ = new PodFSM();
  fsm_->insert(std::make_pair(kPodSchedStagePending, 
                              boost::bind(&PodManager::HandleStagePendingChanged, this, _1)));
  fsm_->insert(std::make_pair(kPodSchedStageRunning, 
                              boost::bind(&PodManager::HandleStageRunningChanged, this, _1)));
  fsm_->insert(std::make_pair(kPodSchedStageRemoved, 
                              boost::bind(&PodManager::HandleStageRemovedChanged, this, _1)));
  job_desc_ = new std::map<std::string, JobSpec>();
  state_to_stage_.insert(std::make_pair(kPodRunning, kPodSchedStageRunning));
  state_to_stage_.insert(std::make_pair(kPodDeploying, kPodSchedStageRunning));
  state_to_stage_.insert(std::make_pair(kPodDeath, kPodSchedStageDeath));
}

PodManager::~PodManager(){}

void PodManager::Start() {
  tpool_.AddTask(boost::bind(&PodManager::WatchJobOp, this));
  tpool_.AddTask(boost::bind(&PodManager::WatchNodeOp, this));
}

void PodManager::WatchNodeOp() {
  NodeStatus* node_status = node_opqueue_->Pop();
  std::map<std::string, PodStatus> pods;
  for (int32_t index = 0; index < node_status->pstatus_size(); ++index) {
    pods.insert(std::make_pair(node_status->pstatus(index).name(), 
                node_status->pstatus(index)));
  }
  SyncPodsOnAgent(node_status->meta().endpoint(), pods);
  tpool_.AddTask(boost::bind(&PodManager::WatchNodeOp, this));
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
      ScaleDownJob(job_op->job_);
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

void PodManager::GetScaleUpPods(const Condition& condition,
                                PodOverviewList* pods) {
  ::baidu::common::MutexLock lock(&mutex_);
  LOG(DEBUG, "get scale up pods, scale_up_jobs size %u", scale_up_jobs_->size());
  const PodJobNameIndex& job_name_index = pods_->get<job_name_tag>();
  std::set<std::string> job_to_remove;
  std::set<std::string>::iterator it = scale_up_jobs_->begin();
  std::set<PodType> require_types;
  for (int32_t index = 0; index < condition.types_size(); ++index) {
    require_types.insert(condition.types(index));
  }

  for (; it != scale_up_jobs_->end(); ++it) {
    std::string job_name = *it;
    JobStat stat;
    bool get_ok = GetJobStatForInternal(job_name, &stat);
    if (!get_ok) {
      LOG(WARNING, "fail get job stat for job %s", job_name.c_str());
      continue;
    } 
    if (stat.pending_ <= 0) {
      // remove job from scale up queue
      job_to_remove.insert(job_name);
      continue;
    }
    int32_t deploy_step_size = 0;
    std::map<std::string, JobSpec>::iterator job_it = job_desc_->find(job_name);
    if (job_it == job_desc_->end()) {
      LOG(WARNING, "fail to get job desc for job %s", job_name.c_str());
      continue;
    }
    if (!condition.job_name_contain().empty()) {
      if (job_name.find(condition.job_name_contain()) == std::string::npos) {
        LOG(DEBUG, "job name %s does not contain %s", job_name.c_str(), condition.job_name_contain().c_str());
        continue;
      }
    }
    deploy_step_size = job_it->second.deploy_step_size();
    LOG(DEBUG, "job %s deploy step size %d stat: pending %d,deploying %d, running %d, death %d", 
        job_name.c_str(),
        deploy_step_size,
        stat.pending_,
        stat.deploying_, 
        stat.running_,
        stat.death_);
    // scan pod with the same job name
    PodJobNameIndex::const_iterator job_name_it = job_name_index.find(job_name);
    // the death count +  deploying count 
    int32_t has_been_scheduled_count = stat.deploying_ + stat.death_;
    for (; job_name_it != job_name_index.end() && has_been_scheduled_count < deploy_step_size;
          ++job_name_it) {
      if (job_name_it->job_name_ != job_name) {
        // the end of scan
        break;
      }
      if (job_name_it->pod_->stage() != kPodSchedStagePending) {
        continue;
      }
      if (require_types.size() > 0) {
        if (require_types.find(job_name_it->pod_->desc().type()) 
            == require_types.end()) {
          break;
        }
      }
      Resource total;
      bool plus_ok = true;
      for (int index = 0; index < job_name_it->pod_->desc().containers_size(); index ++) {
        plus_ok = ResourceUtil::Plus(job_it->second.pod().containers(index).requirement(), &total);
        if (!plus_ok) {
          LOG(WARNING, "fail to calc resource for job %s", job_name_it->name_.c_str());
          break;
        }
      }
      if (!plus_ok) {
        continue;
      }
      LOG(DEBUG, "return pod %s to scheduler", job_name_it->pod_->name().c_str());
      has_been_scheduled_count ++;
      PodOverview* pod_overview = pods->Add();
      pod_overview->set_name(job_name_it->name_);
      pod_overview->set_job_name(job_name);
      pod_overview->set_type(job_name_it->pod_->desc().type());
      // assign sched time
      job_name_it->pod_->set_sched_time(::baidu::common::timer::get_micros());
            pod_overview->mutable_requirement()->CopyFrom(total);
    }
  }
  std::set<std::string>::iterator job_to_remove_it = job_to_remove.begin();
  for (; job_to_remove_it != job_to_remove.end(); ++job_to_remove_it) {
    std::string job_name = *job_to_remove_it;
    LOG(INFO, "remove job %s from scale up queue", job_name.c_str());
    scale_up_jobs_->erase(job_name);
  }

}

void PodManager::HandleStageRemovedChanged(const Event& e) {
  mutex_.AssertHeld();
  PodNameIndex& name_index = pods_->get<name_tag>();
  const std::string pod_name = boost::get<0>(e);
  const PodSchedStage& to_stage = boost::get<2>(e);
  PodNameIndex::iterator name_it = name_index.find(pod_name);
  if (name_it == name_index.end()) {
    LOG(WARNING, "no pod with name %s in pod manager", pod_name.c_str());
    return;
  }
  if (to_stage == kPodSchedStageDeath
      || to_stage == kPodSchedStageRunning) {
    LOG(INFO, "wait pod %s to be killed on agent %s",
        name_it->pod_->name().c_str(),
        name_it->pod_->endpoint().c_str());
    return;
  }else {
    LOG(INFO, "delete pod %s from agent %s", 
        name_it->pod_->name().c_str(),
        name_it->pod_->endpoint().c_str());
    delete name_it->pod_;
    name_index.erase(pod_name);
  }
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
    scale_up_jobs_->insert(name_it->job_name_);
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
    // init a start state  
    name_it->pod_->set_state(kPodDeploying);
  } else if (to_stage == kPodSchedStageRemoved) {
    // remove pod with pending stage , just clean it
    LOG(INFO, "delete pod %s", pod_name.c_str());
    // free pod status
    delete name_it->pod_;
    name_index.erase(name_it);
  } else {
    LOG(WARNING, "[alert]invalidate incoming stage %s with pod %s",
        PodSchedStage_Name(to_stage).c_str(),
        pod_name.c_str()); 
  }
}

bool PodManager::GetJobStatForInternal(const std::string& job_name,
                                       JobStat* stat) {
  if (stat == NULL) {
    LOG(WARNING, "stat is null");
    return false;
  }
  mutex_.AssertHeld();
  const PodJobNameIndex& job_name_index = pods_->get<job_name_tag>();
  PodJobNameIndex::const_iterator it = job_name_index.find(job_name);
  // scan all pods with the same job name 
  for (; it != job_name_index.end(); ++it) {
    if (it->job_name_ != job_name) {
      break;
    }
    switch (it->pod_->state()) {
      // the pending is on agent so 
      case kPodPending:
        stat->pending_++;
        break;
      case kPodDeploying:
        stat->deploying_++;
        break;
      case kPodRunning:
        stat->running_++;
        break;
      case kPodDeath:
        stat->death_++;
        break;
    }
  }
  LOG(INFO, "job %s stat pending_ %d, deploying %d, running %d ,death %d",
      job_name.c_str(), stat->pending_,
      stat->deploying_, stat->running_,
      stat->death_);
  return true;
}

bool PodManager::ScaleDownJob(const JobStatus* job) {
  ::baidu::common::MutexLock lock(&mutex_);
  JobStat stat;
  bool get_ok = GetJobStatForInternal(job->name(), &stat);
  if (!get_ok) {
    LOG(WARNING, "fail to get job %s stat", job->name().c_str());
    return false;
  }
  uint32_t current_replica = stat.pending_ + stat.deploying_ + stat.running_ + stat.death_;
  if (current_replica <= job->desc().replica()) {
    LOG(WARNING, "job %s has no need to make a scaledown", job->name().c_str());
    return true;
  }
  int32_t need_scale_down_count = current_replica - job->desc().replica();
  // the count of pod that being pending will be scaled down
  int32_t pending_count = 0;
  // the count of pod that being death will be scaled down
  int32_t death_count = 0;
  // the count of pod that being deploying will be scaled down
  int32_t deploying_count = 0;
  // the count of pod that being runing will be scaled down
  int32_t running_count = 0;
  do {
    if (need_scale_down_count <= stat.pending_) {
      pending_count = need_scale_down_count;
      break;
    }
    pending_count = stat.pending_;
    need_scale_down_count -= stat.pending_;

    if (need_scale_down_count <= stat.death_) {
      death_count = need_scale_down_count;
      break;
    }
    death_count = stat.death_;
    need_scale_down_count -= stat.death_;

    if (need_scale_down_count <= stat.deploying_) {
      deploying_count = need_scale_down_count;
      break;
    }
    deploying_count = stat.deploying_;
    need_scale_down_count -= stat.deploying_;
    if (need_scale_down_count <= stat.running_) {
      running_count = need_scale_down_count;
      break;
    }
    assert(0);
  }while(false);

  const PodJobNameIndex& job_name_index = pods_->get<job_name_tag>();
  PodJobNameIndex::const_iterator it = job_name_index.find(job->name());
  std::set<std::string> to_be_removed;
  // scan all pods with the same job name 
  for (; it != job_name_index.end(); ++it) {
    if (it->job_name_ != job->name()) {
      break;
    } 
    bool need_send_op = false;
    PodOperation* pod_op = new PodOperation();
    pod_op->type_ = kKillPod;
    pod_op->pod_ = it->pod_;
    switch (it->pod_->state()) {
      // the pending is on agent so 
      case kPodPending:
        // remove it directly in memory
        if (pending_count > 0) {
          to_be_removed.insert(it->pod_->name());
          // free the memory that podstatus occupied
          delete it->pod_;
        }
        break;
      case kPodDeploying:
        if (deploying_count > 0) {
          it->pod_->set_stage(kPodSchedStageRemoved);
          need_send_op = true;
          --deploying_count;
          need_send_op = true;
        }
        break;
      case kPodRunning:
        if (running_count > 0) {
          it->pod_->set_stage(kPodSchedStageRemoved);
          --running_count;
          need_send_op = true;
        }
        break;
      case kPodDeath:
        if (death_count > 0) {
          it->pod_->set_stage(kPodSchedStageRemoved);
          --death_count;
          need_send_op = true;
        }
        break;
    }
    if (need_send_op) {
      pod_opqueue_->Push(pod_op);
    } else {
      delete pod_op;
    }
  }
  PodNameIndex& name_index = pods_->get<name_tag>();
  std::set<std::string>::iterator rm_it = to_be_removed.begin();
  for (; rm_it != to_be_removed.end(); ++rm_it) {
    name_index.erase(*rm_it);
  }
  return false;
}

bool PodManager::GetJobStat(const std::string& job_name,
                            JobStat* stat) {
  ::baidu::common::MutexLock lock(&mutex_);
  return GetJobStatForInternal(job_name, stat);
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
  LOG(DEBUG, "create pods for job %s", job_name.c_str());
  const PodNameIndex& name_index = pods_->get<name_tag>();
  std::vector<std::string> avilable_name;
  for (int32_t offset = 0; offset < replica; ++offset) {
    std::string pod_name = boost::lexical_cast<std::string>(offset) + "_pod." + job_name;
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
    pod->set_job_name(job_name);
    pod->set_sched_time(::baidu::common::timer::get_micros());
    PodIndex pod_index;
    pod_index.name_ = pod->name();
    pod_index.user_name_ = user_name;
    pod_index.job_name_ = job_name;
    pod_index.pod_ = pod;
    pods_->insert(pod_index);
    LOG(INFO, "add new pod %s", pod->name().c_str());
  }
  scale_up_jobs_->insert(job_name); 
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
      LOG(WARNING, "pod %s lost from agent %s", endpoint_it->name_.c_str(), endpoint.c_str());
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

void PodManager::MergePodStatus(const PodStatus& pod_on_agent,
                                 PodStatus* pod_on_master) {
  mutex_.AssertHeld();
  if (pod_on_master == NULL) {
    LOG(WARNING, "pod_on_master is NULL");
    return;
  }
  pod_on_master->set_boot_time(pod_on_agent.boot_time());
  pod_on_master->set_endpoint(pod_on_agent.endpoint());
  pod_on_master->set_state(pod_on_agent.state());
  pod_on_master->mutable_cstatus()->CopyFrom(pod_on_agent.cstatus());
}

}// namespace dos
