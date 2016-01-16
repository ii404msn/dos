// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "engine/initd.h"

#include <sstream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "logging.h"
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
namespace dos {

InitdImpl::InitdImpl():tasks_(), mutex_(), workers_(4), proc_mgr_(){}

InitdImpl::~InitdImpl(){}

void InitdImpl::Fork(RpcController*,
                     const ForkRequest* request,
                     ForkResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::string id;
  bool ok = Launch(request->task(), &id);
  if (!ok) {
      response->set_status(kInitError);
  }else { 
      response->set_status(kInitOk);
      response->set_id(id);
  }
  done->Run();
}

bool InitdImpl::Launch(const Task& task, std::string* id) {
  mutex_.AssertHeld();
  Process p;
  p.cmd_ = task.args();
  p.user_ = task.user();
  p.pty_ = task.pty();
  p.cwd_ = task.cwd();
  bool ok = proc_mgr_.Exec(p, id);
  if (!ok) {
    LOG(WARNING, "fail to create task for args %s", task.args().c_str());
    return false;
  }else {
    LOG(INFO, "launch task %s with args %s successfully", id->c_str(), task.args().c_str());
  }
  TaskInfo* task_info = new TaskInfo();
  task_info->task_ = task;
  task_info->status_.set_id(*id);
  task_info->status_.set_state(kTaskRunning);
  task_info->status_.set_created(::baidu::common::timer::get_micros());
  task_info->status_.set_user(task.user());
  task_info->status_.set_args(task.args());
  tasks_.insert(std::make_pair<std::string, TaskInfo*>(*id, task_info));
  workers_.DelayTask(2000, boost::bind(&InitdImpl::CheckStatus, this, *id));
  return true;
}

void InitdImpl::CheckStatus(const std::string& id) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::map<std::string, TaskInfo*>::iterator it = tasks_.find(id);
  if (it == tasks_.end()) {
    LOG(WARNING, "fail to find task with id %s", id.c_str());
    return;
  }
  Process p;
  bool ok = proc_mgr_.Wait(id, &p);
  if (!ok) {
    LOG(WARNING, "fail to wait task %s", id.c_str());
    // clean task in proc_mgr;
    proc_mgr_.Kill(id, 9);
    return;
  }
  if (p.running_) {
    it->second->status_.set_state(kTaskRunning);
    workers_.DelayTask(2000, boost::bind(&InitdImpl::CheckStatus, this, id));
    LOG(INFO, "task witd id %s is running", id.c_str());
  }else {
    if (p.coredump_) {
      it->second->status_.set_state(kTaskCore);
    }else if(p.ecode_ == 0) {
      it->second->status_.set_state(kTaskComplete);
    }else {
      it->second->status_.set_state(kTaskUnknown);
    }
    LOG(INFO, "task witd id %s is dead", id.c_str());
  }
}

void InitdImpl::Wait(RpcController* controller,
                     const WaitRequest* request,
                     WaitResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  for (int i = 0; i < request->ids_size(); i++) {
    std::map<std::string, TaskInfo*>::iterator it = tasks_.find(request->ids(i));
    if (it == tasks_.end()) {
      LOG(WARNING, "task with id %s does not exist in initd", request->ids(i).c_str());
      continue;
    }
    TaskStatus* status = response->add_task_status();
    status->CopyFrom(it->second->status_);
  }
  response->set_status(kInitOk);
  done->Run();
}

void InitdImpl::Kill(RpcController* controller,
                     const KillRequest* request,
                     KillResponse* response,
                     Closure* done) {
  ::baidu::common::MutexLock lock(&mutex_);
  std::set<std::string> will_be_removed;
  for (int i = 0; i < request->ids_size(); i++) {
    std::string id = request->ids(i);
    std::map<std::string, TaskInfo*>::iterator it = tasks_.find(id);
    if (it == tasks_.end()) {
      LOG(WARNING, "task with id %s does not exist in initd", id.c_str());
      continue;
    }
    bool ok = proc_mgr_.Kill(id, request->signal());
    if (ok) {
      LOG(INFO, "kill task with id %s successfully", id.c_str());
      will_be_removed.insert(id);
      continue;
    }
    LOG(WARNING, "killing task with id %s fails", id.c_str());
  }
  response->set_status(kInitOk);
  done->Run();
  std::set<std::string>::iterator it = will_be_removed.begin();
  for (; it != will_be_removed.end(); ++it) {
    TaskInfo* task_info = tasks_[*it];
    delete task_info;
    tasks_.erase(*it);
  }
}

}// namespace dos
