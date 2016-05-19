#ifndef KERNEL_ENGINE_PACKAGE_MGR_H
#define KERNEL_ENGINE_PACKAGE_MGR_H

#include <leveldb/db.h>
#include "proto/dos.pb.h"
#include "proto/initd.pb.h"
#include "engine/process_mgr.h"
#include "rpc/rpc_client.h"
#include "mutex.h"

using ::baidu::common::Mutex;
using ::baidu::common::MutexLock;

namespace dos {

struct PackageDescriptor {
  std::string uri;
  ContainerType type;
};

class PackageMgr {

public:
  PackageMgr(leveldb::DB* db);
  ~PackageMgr();

  bool Init();
  bool CheckInitd();

private:
  bool LaunchInitd();

private:
  leveldb::DB* db_;
  Mutex mutex_;
  ProcessMgr* proc_mgr_;
  std::string endpoint_;
  Initd_Stub* initd_stub_;
  RpcClient* rpc_client_;
};

}
#endif
