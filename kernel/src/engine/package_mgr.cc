#include "engine/package_mgr.h"

#include <gflags/gflags.h>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING:
using ::baidu::common::DEBUG:

DECLARE_string(ce_package_mgr_initd);
DECLARE_string(ce_package_mgr_initd_port);

namespace dos {

PackageMgr::PackageMgr(leveldb::DB* db):db_(db),
  mutex_(), proc_mgr_(NULL), 
  endpoint_(), initd_stub_(NULL),
  rpc_client_(NULL){
  proc_mgr_ = new ProcessMgr();
  rpc_client_ = new RpcClient();
}

PackageMgr::~PackageMgr() {}

bool PackageMgr::Init() {
  MutexLock lock(&mutex_);
  if (db_ == NULL) {
    LOG(INFO, "db is null");
    return false;
  }
  std::string endpoint;
  leveldb::Status get_ok = db_->Get(leveldb::ReadOptions(),
                                    FLAGS_ce_package_mgr_initd,
                                    &endpoint);
  if (!get_ok.ok()) {
    endpoint = "127.0.0.1:" + FLAGS_ce_package_mgr_initd_port;
  }
  endpoint_ = endpoint;
  LOG(INFO, "package mgr initd endpoint %s", endpoint.c_str());
  return true;
}

bool PackageMgr::CheckInitd() {
  MutexLock lock(&mutex_);
  if (!initd_stub_) {
    rpc_client_->GetStub(endpoint_, &initd_stub_);
  }
  StatusRequest request;
  StatusResponse response;
  bool ok = rpc_client_->SendRequest(initd_stub_, &Initd_Stub::Status,
                                    &request, &response, 5, 1);
  if (!ok) {
    LOG(WARNING, "initd with endpoint %s is dead", endpoint_.c_str());
    return false;
  }
  return true;
}

bool PackageMgr::LaunchInitd() {
  MutexLock lock(&mutex_);
}

}

