// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <map>
#include <unistd.h>
#include <string>
#include <stdint.h>
#include "proto/dos.pb.h"

namespace dos {

//NOTE not thread safe
class ProcessMgr {

public:
  ProcessMgr();
  ~ProcessMgr();
  bool Exec(const Process& process);
  bool Wait(const std::string& name, Process* process);
  bool Kill(const std::string& name, int signal);
private:
  bool GetOpenedFds(std::set<int>& fds);
  bool ResetIo(const Process& process);
  bool GetUser(const std::string& user, 
               int32_t* uid,
               int32_t* gid);
private:
  std::map<std::string, Process>* processes_;
  pid_t mypid_;
};

}
