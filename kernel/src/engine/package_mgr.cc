#include "engine/package_mgr.h"

#include <gflags/gflags.h>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING:
using ::baidu::common::DEBUG:

DECLARE_string(ce_bin_path);
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
    leveldb::Status put_ok = db_->Put(leveldb::WriteOptions(), 
                                      FLAGS_ce_package_mgr_initd,
                                      endpoint);
    if (!put_ok.ok()) {
      LOG(INFO, "fail to write initd endpoint for package mgr");
      return false;
    }
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
  mutex_.AssertHeld();

}

bool PackageMgr::BuildInitdFlags(const std::string& work_dir) {
  std::string flag_path = work_dir + "/initd.flags";
  std::ofstream flags(flag_path.c_str(), 
                      std::ofstream::trunc);
  if (!flags.is_open()) {
    LOG(WARNING, "fail to open %s ", flag_path.c_str());
    return false;
  }
  flags << "--ce_enable_ns=false\n";
  flags << "--ce_container_name=packagemgr\n";
  flags << "--ce_cgroup_root=" << FLAGS_ce_cgroup_root << "\n";
  flags << "--ce_isolators=" << FLAGS_ce_isolators << "\n";
  flags << "--ce_initd_port=" << FLAGS_ce_package_mgr_initd_port << "\n";
  flags.close();
}

}

