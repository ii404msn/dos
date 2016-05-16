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
  mutex_(){}

PackageMgr::~PackageMgr() {}

bool PackageMgr::Init() {
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
  LOG(INFO, "package mgr initd endpoint %s", endpoint.c_str()):
}

}

