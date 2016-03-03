// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef KERNEL_ENGINE_PROCESS_MGR_H
#define KERNEL_ENGINE_PROCESS_MGR_H
#include <set>
#include <map>
#include <unistd.h>
#include <string>
#include <stdint.h>
#include <set>
#include "proto/dos.pb.h"
#include "engine/user_mgr.h"
#include "dsh/dsh.h"

namespace dos {

struct CloneContext {
  std::set<int> fds;
  std::string job_desc;
};

class ProcessMgr {

public:
  ProcessMgr();
  ~ProcessMgr();
  // after exec a process , kill method must be invoked for free process data 
  bool Exec(const Process& process);
  bool Clone(const Process& process, int flag);
  bool Wait(const std::string& name, Process* process);
  // kill process and clean data
  bool Kill(const std::string& name, int signal);
private:
  static bool GetOpenedFds(std::set<int>& fds);
  static bool GetUser(const std::string& user, 
                      int32_t* uid,
                      int32_t* gid);
  static int LaunchProcess(void* args);
private:
  std::map<std::string, Process>* processes_;
  Dsh* dsh_;
};

}
#endif
