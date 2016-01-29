#ifndef KERNEL_MASTER_POD_MANAGER_H
#define KERNEL_MASTER_POD_MANAGER_H

#include <vector>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/tuple/tuple.hpp>

#include "master/idx_tag.h"
#include "master/blocking_queue.h"
#include "master/master_internal_types.h"
#include "mutex.h"
#include "thread_pool.h"

namespace dos {

struct PodIndex {
  std::string name_;
  std::string job_name_;
  std::string user_name_;
  std::string endpoint_;
  PodStatus* pod_;
};

typedef boost::multi_index_container<
  PodIndex,
  boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<
      boost::multi_index::tag<name_tag>,
      BOOST_MULTI_INDEX_MEMBER(PodIndex , std::string, name_)
    >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<job_name_tag>,
      BOOST_MULTI_INDEX_MEMBER(PodIndex, std::string, job_name_)
    >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<user_name_tag>,
      BOOST_MULTI_INDEX_MEMBER(PodIndex, std::string, user_name_)
    >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<endpoint_tag>,
      BOOST_MULTI_INDEX_MEMBER(PodIndex, std::string, endpoint_)
    >
  >
> PodSet;


typedef boost::multi_index::index<PodSet, job_name_tag>::type PodJobNameIndex;
typedef boost::multi_index::index<PodSet, name_tag>::type PodNameIndex;
typedef boost::multi_index::index<PodSet, user_name_tag>::type PodUserNameIndex;
typedef boost::multi_index::index<PodSet, endpoint_tag>::type PodEndpointIndex;

// the event leads pod to change it's stage,
// the tuple[0] is pod name , tuple[1] is current stage, tuple[2] is targe stage
typedef boost::tuple<std::string, PodSchedStage, PodSchedStage> Event;

typedef boost::function<void (const Event& e)> PodEventHandle;
// the current stage handle event pair
typedef std::map<PodSchedStage, PodEventHandle>  PodFSM;

class PodManager {

public:
  PodManager(FixedBlockingQueue<PodOperation*>* pod_opqueue,
             FixedBlockingQueue<JobOperation*>* job_opqueue);
  ~PodManager();
  void Start();
  // sync the pods on agent
  void SyncPodsOnAgent(const std::string& endpoint,
                       std::map<std::string, PodStatus>& pods);
  // sched pod, the tuple first arg is endpoint, the second is pod name
  void SchedPods(const std::vector<boost::tuple<std::string, std::string> >& pods);
private:
  void WatchJobOp();
  // create new pods
  bool NewAdd(const std::string& job_name,
              const std::string& user_name,
              const PodSpec& desc,
              int32_t replica);
  void DispatchEvent(const Event& e);
  void HandleStageRunningChanged(const Event& e);
  void HandleStagePendingChanged(const Event& e);
private:
  PodSet* pods_;
  std::set<std::string>* scale_up_pods_;
  std::set<std::string>* scale_down_pods_;
  ::baidu::common::Mutex mutex_;
  PodFSM* fsm_;
  std::map<PodState, PodSchedStage> state_to_stage_;
  FixedBlockingQueue<PodOperation*>* pod_opqueue_;
  FixedBlockingQueue<JobOperation*>* job_opqueue_;
  ::baidu::common::ThreadPool tpool_;
};

}// namespace dos
#endif
