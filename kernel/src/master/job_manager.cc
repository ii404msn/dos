#include "master/job_manager.h"

#include "logging.h"
#include "timer.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;

namespace dos {

JobManager::JobManager(FixedBlockingQueue<JobOperation*>* job_opqueue):jobs_(NULL),
  mutex_(),
  job_opqueue_(job_opqueue){
  jobs_ = new JobSet();
}

JobManager::~JobManager() {
  delete jobs_;
}

void JobManager::CleanJob(const std::string& name) {
  ::baidu::common::MutexLock lock(&mutex_); 
  JobNameIndex& name_index = jobs_->get<name_tag>();
  name_index.erase(name);
}

bool JobManager::GetJob(const std::string& name,
                        JobOverview* job) {
  ::baidu::common::MutexLock lock(&mutex_); 
  const JobNameIndex& name_index = jobs_->get<name_tag>();
  JobNameIndex::const_iterator name_it = name_index.find(name);
  if (name_it == name_index.end()) {
    return false; 
  }
  job->set_name(name_it->name_);
  job->set_replica(name_it->job_->desc().replica());
  job->set_deploy_step(name_it->job_->desc().deploy_step_size());
  job->set_state(JobState_Name(name_it->job_->state()));
  job->set_ctime(name_it->job_->ctime());
  job->set_utime(name_it->job_->utime());
  return true;
}

bool JobManager::Add(const std::string& user_name,
                     const JobSpec& desc) {
  ::baidu::common::MutexLock lock(&mutex_); 
  const JobNameIndex& name_index = jobs_->get<name_tag>();
  JobNameIndex::const_iterator name_it = name_index.find(desc.name());
  if (name_it != name_index.end()) {
    LOG(WARNING, "job with name %s does exist", desc.name().c_str());
    return false;
  }
  JobStatus* job_status = new JobStatus();
  job_status->mutable_desc()->CopyFrom(desc);
  job_status->set_ctime(::baidu::common::timer::get_micros());
  job_status->set_utime(::baidu::common::timer::get_micros());
  job_status->set_user_name(user_name);
  job_status->set_state(kJobNormal);
  job_status->set_name(desc.name());
  JobIndex job_index;
  job_index.name_ = desc.name();
  job_index.user_name_ = user_name;
  job_index.job_ = job_status;
  jobs_->insert(job_index);
  LOG(INFO, "user %s create job %s successfully", user_name.c_str(),
      desc.name().c_str());
  JobOperation* op = new JobOperation();
  op->job_ = job_status;
  op->type_ = kJobNewAdd;
  job_opqueue_->Push(op);
  return true;
}

bool JobManager::KillJob(const std::string& name) {
  const JobNameIndex& name_index = jobs_->get<name_tag>();
  JobNameIndex::const_iterator name_it = name_index.find(name);
  if (name_it == name_index.end()) {
    return false; 
  }
  name_it->job_->set_state(kJobRemoved);
  name_it->job_->mutable_desc()->set_replica(0);
  JobOperation* op = new JobOperation();
  op->job_ = name_it->job_;
  op->type_ = kJobRemove;
  job_opqueue_->Push(op);
  return true;
}

}// namespace dos
