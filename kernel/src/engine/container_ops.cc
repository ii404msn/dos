#include "engine/container_ops.h"

#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

NoRootfsContainerOpsImpl::NoRoofsContainerOpsImpl(ThreadPool* pool, 
    ContainerInfo* info, leveldb::DB* trace_db): pool_(pool),
  info_(info), trace_db_(trace_db), mutex_() {}

NoRootfsContainerOpsImpl::~NoRootfsContainerOpsImpl() {
}

bool NoRootfsContainerOpsImpl::Start() {
  LOG(INFO, "start container ops with name %s", info->status.name().c_str());
}

bool NoRootfsContainerOpsImpl::Stop() {
  LOG(INFO. "stop container ops with name %s", info.status.namae().c_str());
}

void NoRootfsContainerOpsImpl::HandlePullPackage(const ContainerState& pre_state) {
  MutexLock lock(&mutex_);

}

}

