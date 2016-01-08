#include "master/node_manager.h"

#include "logging.h"

namespace dos {

NodeManager::NodeManager():mutex_(),
  nodes_(NULL),
  nexus_(NULL),
  node_metas_(NULL){
  nodes_ = new NodeSet();
  nexus_ = new ::galaxy::ins::sdk::InsSDK();
  node_metas_ = new boost::unordered_map<std::string, NodeMeta*>();
}

NodeManager::~NodeManager() {
  delete node_set_;
}

bool NodeManager::LoadNodeMeta() {
  MutexLock lock(&mutex_);

}

} // end of dos
