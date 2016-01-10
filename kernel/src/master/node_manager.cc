#include "master/node_manager.h"

#include <map>
#include <gflags/gflags.h>
#include "logging.h"
#include "timer.h"

DECLARE_string(nexus_servers);
DECLARE_string(master_root_path);
DECLARE_string(master_node_path_prefix);

namespace dos {

NodeManager::NodeManager():mutex_(),
  nodes_(NULL),
  nexus_(NULL),
  node_metas_(NULL){
  nodes_ = new NodeSet();
  nexus_ = new ::galaxy::ins::sdk::InsSDK(FLAGS_nexus_servers);
  node_metas_ = new boost::unordered_map<std::string, NodeMeta*>();
}

NodeManager::~NodeManager() {
  delete nodes_;
  delete nexus_;
  delete node_metas_;
}

bool NodeManager::LoadNodeMeta() {
  MutexLock lock(&mutex_);
  std::string start_key = FLAGS_master_root_path + FLAGS_node_path_prefix + "/";
  std::string end_key = start_key + "~";
  ::galaxy::ins::sdk::ScanResult* result = nexus_->Scan(start_key, end_key);
  while (!result->Done()) {
    assert(result->Error() == ::galaxy::ins::sdk::kOK);
    std::string key = result->Key();
    std::string node_raw_data = result->Value();
    NodeMeta* meta = new NodeMeta();
    bool ok = mete->ParseFromString(node_raw_data);
    if (!ok) {
      LOG(WARNING, "fail to load node with key %s", key.c_str());
      assert(0);
    }
    node_metas_->insert(std::make_pair(meta->hostname(), meta));
    result->Next();
  }
  LOG(INFO, "load node meta with count %u", node_metas_->size());
  return true;
}


void NodeManager::KeepAlive(const std::string& hostname,
                            const std::string& endpoint) {
  MutexLock lock(&mutex_);
  const NodeEndpointIndex& endpoint_idx = nodes_->get<endpoint_tag>();
  NodeEndpointIndex::const_iterator endpoint_it = endpoint_idx.find(endpoint);
  if (endpoint_it == endpoint_idx.end()) {
    LOG(INFO, "new node %s heart beat with endpoint %s", hostname.c_str(), endpoint.c_str());
    NodeIndex index;
    index.meta_ = NULL;
    index.hostname_ = hostname;
    index.endpoint_ = endpoint;
    index.used_ = NULL;
    boost::unordered_map<std::string, NodeMeta*>::iterator node_it = node_metas_->find(hostname);
    if (node_it == node_metas_->end()) {
      LOG(WARNING, "node %s has no meta in master", hostname.c_str());
    }else {
      index.meta_ = node_it->second;
    }
  }
}

} // end of dos
