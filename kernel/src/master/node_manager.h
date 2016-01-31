#ifndef KERNEL_MASTER_NODE_MANAGER_H
#define KERNEL_MASTER_NODE_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/unordered_map.hpp>

#include "common/blocking_queue.h"
#include "master/master_internal_types.h"
#include "master/idx_tag.h"
#include "mutex.h"
#include "ins_sdk.h"
#include "thread_pool.h"
#include "proto/dos.pb.h"
#include "proto/agent.pb.h"
#include "rpc/rpc_client.h"

namespace dos {

struct NodeIndex {
  std::string hostname_;
  std::string endpoint_;
  NodeMeta* meta_;
  NodeStatus* status_;
};

typedef boost::multi_index_container<
     NodeIndex,
     boost::multi_index::indexed_by<
          boost::multi_index::hashed_unique<
             boost::multi_index::tag<hostname_tag>,
             BOOST_MULTI_INDEX_MEMBER(NodeIndex , std::string, hostname_)
         >,
         boost::multi_index::hashed_unique<
             boost::multi_index::tag<endpoint_tag>,
             BOOST_MULTI_INDEX_MEMBER(NodeIndex, std::string, endpoint_)
        >
    >
> NodeSet;

typedef boost::multi_index::index<NodeSet, hostname_tag>::type NodeHostnameIndex;
typedef boost::multi_index::index<NodeSet, endpoint_tag>::type NodeEndpointIndex;

class NodeManager {

public:
  NodeManager(FixedBlockingQueue<NodeStatus*>* node_status_queue,
              FixedBlockingQueue<PodOperation*>* pod_opqueue);
  ~NodeManager();
  bool LoadNodeMeta();
  void KeepAlive(const std::string& hostname, 
                 const std::string& endpoint);
  void QueryNode(const std::string& endpoint);

private:
  void HandleNodeTimeout(const std::string& endpoint);
  void WatchPodOpQueue();
  void RunPod(const std::string& pod_name,
              const std::string& endpoint,
              const PodSpec& desc);
  void RunPodCallback(const RunPodRequest* request,
                      RunPodResponse* response,
                      bool failed, int);
private:
  ::baidu::common::Mutex mutex_;
  NodeSet* nodes_;
  ::galaxy::ins::sdk::InsSDK* nexus_;
  // hostname and NodeMeta pair
  boost::unordered_map<std::string, NodeMeta*>* node_metas_;
  // endpoint and agent_stub pair
  boost::unordered_map<std::string, Agent_Stub*>* agent_conns_;
  ::baidu::common::ThreadPool* thread_pool_;
  FixedBlockingQueue<NodeStatus*>* node_status_queue_;
  FixedBlockingQueue<PodOperation*>* pod_opqueue_;
  RpcClient* rpc_client_;
};

} // end of dos
#endif
