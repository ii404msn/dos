#ifndef KERNEL_ENGINE_CONTAINER_H
#define KERNEL_ENGINE_CONTAINER_H

#include <boost/function.hpp>
#include <leveldb/db.h>
#include "proto/dos.pb.h"
#include "thread_pool.h"
#include "mutex.h"

using ::baidu::common::ThreadPool;
using ::baidu::common::Mutex;
using ::baidu::common::MutexLock;

namespace dos {

// the container information for ops
class ContainerInfo {

public:
  ContainerInfo();
  ~ContainerInfo();

private:
  ContainerStatus* status_;
};

typedef boost::function<void ()> CleanHook;

// container ops which manages container pull package 
// boot initd, start command and stop container
class ContainerOps {

public:
  // start container ops  
  virtual bool Start() = 0;
  virtual bool Stop() = 0;

  // pull a package and produce a state,
  // if pulling is successfully, produce a kContainerRunning state 
  // or produce a kContainerPulling when retry is in limit
  // or produce a kContainerPullFailed 
  virtual void HandlePullPackage(const ContainerState& pre_state) = 0;

  // boot container initd process
  virtual void HandleBootInitd(const ContainerState& pre_state) = 0;

};

class NoRootfsContainerOpsImpl : public ContainerOps {

public:

  NoRootfsContainerOpsImpl(ThreadPool* pool,
                           ContainerInfo* info,
                           leveldb::DB* trace_db);

  ~NoRootfsContainerOpsImpl();

  bool Start();
  bool Stop();
  void HandlePullPackage(const ContainerState& pre_state);
  void HandleBootInitd(const ContainerState& pre_state);

private:
  
  // pull package logic
  bool SendPullPackageCmd();
  bool GetPullPackageState();

private:
  CleanHook hook_;
  ThreadPool* pool_;
  ContainerInfo* info_;
  // trace event and stat of container
  leveldb::DB* trace_db_;
  Mutex mutex_;
};

}

#endif
