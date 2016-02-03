#ifndef KERNEL_AGENT_IMPL_H
#define KERNEL_AGENT_IMPL_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include "proto/agent.pb.h"

#include "thread_pool.h"
#include "proto/master.pb.h"
#include "proto/engine.pb.h"
#include "rpc/rpc_client.h"
#include "mutex.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;

namespace dos {

// container name tag
struct c_name_tag {};
// container pod name tag
struct p_name_tag {};

struct ContainerIdx {
  std::string name_;
  std::string pod_name_;
  ContainerStatus* status_;
  Container* desc_;
  std::deque<PodLog>* logs_;
  ContainerIdx():name_(), pod_name_(),
  status_(NULL), logs_(NULL){}
  ~ContainerIdx(){}
};

typedef boost::multi_index_container<
  ContainerIdx,
  boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<
      boost::multi_index::tag<c_name_tag>,
      BOOST_MULTI_INDEX_MEMBER(ContainerIdx, std::string, name_)
    >,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<p_name_tag>,
      BOOST_MULTI_INDEX_MEMBER(ContainerIdx, std::string, pod_name_)
    >
  >
> ContainerSet;

typedef boost::multi_index::index<ContainerSet, c_name_tag>::type ContainerNameIdx;
typedef boost::multi_index::index<ContainerSet, p_name_tag>::type ContainerPodNameIdx;

class AgentImpl : public Agent{

public:
  AgentImpl();
  ~AgentImpl();
  void Poll(RpcController* controller,
            const PollAgentRequest* request,
            PollAgentResponse* response,
            Closure* done);
  void Run(RpcController* controller,
           const RunPodRequest* request,
           RunPodResponse* response,
           Closure* done);
  bool Start();
private:
  void HeartBeat();
  void HeartBeatCallback(const HeartBeatRequest* request,
                         HeartBeatResponse* response,
                         bool failed, int);
  void HandleRunContainer(const std::string& c_name);
private:
  ::baidu::common::ThreadPool thread_pool_;
  Master_Stub* master_;
  RpcClient* rpc_client_;
  ::baidu::common::Mutex mutex_;
  ContainerSet* c_set_;
  Engine_Stub* engine_;
};

}// end of dos
#endif
