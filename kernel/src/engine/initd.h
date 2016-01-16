// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef KERBEL_ENGINE_INITD_H
#define KERNEL_ENGINE_INITD_H

#include "proto/initd.pb.h"

#include <map>
#include <set>
#include "mutex.h"
#include "thread_pool.h"
#include "engine/process_mgr.h"

using ::google::protobuf::RpcController;
using ::google::protobuf::Closure;
namespace dos{

struct TaskInfo {
    Task task_;
    TaskStatus status_;
};

class InitdImpl : public Initd {

public:
    InitdImpl();
    ~InitdImpl();
    void Fork(RpcController* controller,
              const ForkRequest* request,
              ForkResponse* response,
              Closure* done);
    void Wait(RpcController* controller,
              const WaitRequest* request,
              WaitResponse* response,
              Closure* done);
    void Kill(RpcController* controller,
              const KillRequest* request,
              KillResponse* response,
              Closure* done);
private:
    bool Launch(const Task& task, std::string* id);
    void CheckStatus(const std::string& id);
private:
    std::map<std::string, TaskInfo*> tasks_;
    ::baidu::common::Mutex mutex_;
    ::baidu::common::ThreadPool workers_; 
    ProcessMgr proc_mgr_;
};

} // namespace dos
#endif
