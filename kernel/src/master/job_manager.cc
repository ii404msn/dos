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

}// namespace dos
