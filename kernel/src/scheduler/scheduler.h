#ifndef KERNEL_SCHEDULER_SCHEDULER_H
#define KERNEL_SCHEDULER_SCHEDULER_H

#include <boost/unordered_map.hpp>
#include "rpc/rpc_client.h"
#include "proto/master.pb.h"
#include "thread_pool.h"
#include "mutex.h"

namespace dos {

class Scheduler {

public:
  Scheduler(const std::string& master_addr);
  ~Scheduler();
  bool Start();
private:
  void GetScaleUpPods();
  void SyncAgentInfo();
private:
  RpcClient* rpc_client_;
  Master_Stub* master_;
  std::string master_addr_;
  ::baidu::common::ThreadPool pool_;
  ::baidu::common::Mutex mutex_;
  // endpoint AgentOverview pair 
  boost::unordered_map<std::string, AgentOverview*>* agents_;
};

} //namespace dos
#endif
