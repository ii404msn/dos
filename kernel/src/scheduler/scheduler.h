#ifndef KERNEL_SCHEDULER_SCHEDULER_H
#define KERNEL_SCHEDULER_SCHEDULER_H

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/unordered_map.hpp>
#include "rpc/rpc_client.h"
#include "proto/master.pb.h"
#include "thread_pool.h"
#include "mutex.h"
#include "ins_sdk.h"
#include "common/ins_watcher.h"

namespace dos {

using ::galaxy::ins::sdk::InsSDK;
typedef boost::function<double (const AgentOverview&)> ScoreFunc;

enum SchedAction {
  ScaleUp, ScaleDown
};

struct ProposeCell {
  double score;
  AgentOverview overview;
};

struct SchedCell {
  std::string job_name;
  Resource resource;
  std::vector<std::string> pods;
  int32_t feasibile_count;
  int32_t priority;
  std::vector<ProposeCell> agents;
  SchedAction action;
  PodType type;
  std::map<PodType, ScoreFunc> score_func_map;
  SchedCell(): job_name(), resource(), pods(),
  priority(0), agents(),
  action(ScaleUp){
    score_func_map.insert(std::make_pair(kPodLongrun,
          boost::bind(&SchedCell::ScoreForLongrunPod, this, _1)));
    score_func_map.insert(std::make_pair(kPodSystem,
          boost::bind(&SchedCell::ScoreForLongrunPod, this, _1)));
    score_func_map.insert(std::make_pair(kPodBatch,
          boost::bind(&SchedCell::ScoreForBatchPod, this, _1)));
    score_func_map.insert(std::make_pair(kPodBesteffort,
          boost::bind(&SchedCell::ScoreForBatchPod, this, _1)));
  }
  void Score();
  double ScoreForLongrunPod(const AgentOverview& agent);
  double ScoreForBatchPod(const AgentOverview& agent);
  ~SchedCell(){}
};

class Scheduler {

public:
  Scheduler();
  ~Scheduler();
  bool Start();
private:
  void GetScaleUpPods();
  void SyncAgentInfo();
  void BuildScaleUpSchedCell(const GetScaleUpPodResponse& response,
                             std::map<std::string, SchedCell>& cells);
  void SortSchedCell(const std::map<std::string, SchedCell>& cells,
                     std::vector<SchedCell>& sorted_cells);
  void ProcessScaleUpCell(std::vector<SchedCell>& cells);

  template<class T>
  void Shuffle(std::vector<T>& list) {
    for (size_t i = list.size(); i > 1; i--) {
      T tmp = list[i-1];
      size_t target_index = ::rand() % i ;
      list[i-1] = list[target_index];
      list[target_index] = tmp;
    }
  }
  void ProcessScaleUpPropose(SchedCell cell);
private:
  void HandleMasterChange(const std::string& master_endpoint);
private:
  RpcClient* rpc_client_;
  Master_Stub* master_;
  std::string master_addr_;
  ::baidu::common::ThreadPool pool_;
  ::baidu::common::Mutex mutex_;
  // endpoint AgentOverview pair 
  boost::unordered_map<std::string, AgentOverview*>* agents_;
  InsSDK* ins_;
  InsWatcher* ins_watcher_;
};

} //namespace dos
#endif
