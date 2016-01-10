#ifndef KERNEL_MASTER_NODE_MANAGER_H
#define KERNEL_MASTER_NODE_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/unordered_map.hpp>

#include "master/blocking_queue.h"
#include "master/idx_tag.h"
#include "mutex.h"
#include "ins_sdk.h"
#include "thread_pool.h"
#include "proto/dos.pb.h"

namespace dos {

struct NodeIndex {
  std::string hostname_;
  std::string endpoint_;
  NodeMeta* meta_;
  Resource* used_;
  int64_t timer_id_;
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
  NodeManager(FixedBlockingQueue<NodeStatus*>* node_status_queue);
  ~NodeManager();
  bool LoadNodeMeta();
  void KeepAlive(const std::string& hostname, 
                 const std::string& endpoint);
  void HandleNodeTimeout(const std::string& endpoint);
  void QueryNode(const std::string& endpoint);
private:
  ::baidu::common::Mutex mutex_;
  NodeSet* nodes_;
  ::galaxy::ins::sdk::InsSDK* nexus_;
  // hostname and NodeMeta pair
  boost::unordered_map<std::string, NodeMeta*>* node_metas_;
  ::baidu::common::ThreadPool* thread_pool_;
  FixedBlockingQueue<NodeStatus*>* node_status_queue_;
};

} // end of dos
#endif
