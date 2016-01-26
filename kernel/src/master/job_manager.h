#ifndef KERNEL_MASTER_JOB_MANAGER_H
#define KERNEL_MASTER_JOB_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace dos {

struct JobIndex {
  // the global unique
  std::string name_;
  // the global unique
  std::string user_name_;

};

class JobManager {};

}
#endif
