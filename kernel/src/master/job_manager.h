#ifndef KERNEL_MASTER_JOB_MANAGER_H
#define KERNEL_MASTER_JOB_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include "master/master_internal_types.h"
#include "common/blocking_queue.h"
#include "master/idx_tag.h"
#include "mutex.h"

namespace dos {

struct JobIndex {
  // the global unique
  std::string name_;
  // the global unique
  std::string user_name_;
  JobStatus* job_;
};

typedef boost::multi_index_container<
  JobIndex,
  boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<
      boost::multi_index::tag<name_tag>,
      BOOST_MULTI_INDEX_MEMBER(JobIndex , std::string, name_)
    >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<user_name_tag>,
      BOOST_MULTI_INDEX_MEMBER(JobIndex, std::string, user_name_)
    >
  >
> JobSet;

typedef boost::multi_index::index<JobSet, name_tag>::type JobNameIndex;
typedef boost::multi_index::index<JobSet, user_name_tag>::type JobUserNameIndex;

class JobManager {

public:
  JobManager(FixedBlockingQueue<JobOperation*>* job_opqueue);
  ~JobManager();
  bool Add(const std::string& user_name,
           const JobSpec& desc);
private:
  JobSet* jobs_;
  ::baidu::common::Mutex mutex_;
  FixedBlockingQueue<JobOperation*>* job_opqueue_;
};

}
#endif
