// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "engine/process_mgr.h"

#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sstream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <assert.h>
#include <errno.h>
#include "logging.h"
#include "timer.h"
#include "engine/utils.h"

#define STACK_SIZE (1024 * 1024)
static char CLONE_STACK[STACK_SIZE];
using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

int ProcessMgr::LaunchProcess(void* args) {
  CloneContext* context = reinterpret_cast<CloneContext*>(args);
  std::set<int>::iterator fd_it = context->fds.begin();
  for (; fd_it != context->fds.end(); ++fd_it) {
    int fd = *fd_it;
    ::close(fd);
  }
  char* argv[] = {
      const_cast<char*>("dsh"),
      const_cast<char*>("-f"),
      const_cast<char*>(context->job_desc.c_str()),
      NULL};
  ::execv("/bin/dsh", argv);
  assert(0);
}

ProcessMgr::ProcessMgr():processes_(NULL){
  processes_ = new std::map<std::string, Process>();
  dsh_ = new Dsh();
}

ProcessMgr::~ProcessMgr(){}

int32_t ProcessMgr::Exec(const Process& process) {
  Process local;
  local.CopyFrom(process);
  local.set_rtime(::baidu::common::timer::get_micros());
  if (local.user().uid() < 0) {
    LOG(WARNING, "user is required");
    return -1;
  }
  if (local.cwd().empty()) {
    local.set_cwd("/home/" + process.user().name());
  }
  bool ok = MkdirRecur(local.cwd());
  if (!ok) {
    LOG(WARNING, "fail create workdir %s", local.cwd().c_str());
    return -1;
  }
  std::string job_desc = local.cwd() + "/" + process.name() + "_process.yml";
  bool gen_ok = dsh_->GenYml(local, job_desc);
  if (!gen_ok) {
    LOG(WARNING, "fail to gen yml for process %s", process.name().c_str());
    return -1;
  }
  std::set<int> openfds;
  ok = GetOpenedFds(openfds);
  if (!ok) {
    LOG(WARNING, "fail to get opened fds  ");
    return -1;
  }
  pid_t pid = fork();
  if (pid == -1) {
    LOG(WARNING, "fail to fork process %s", local.name().c_str());
    return -1;
  }else if (pid == 0) {
    // close fds that are copied from parent
    std::set<int>::iterator fd_it = openfds.begin();
    for (; fd_it !=  openfds.end(); ++fd_it) {
      close(*fd_it);
    }
    char* args[] = {
      const_cast<char*>("dsh"),
      const_cast<char*>("-f"),
      const_cast<char*>(job_desc.c_str()),
      NULL
    };
    ::execv("/bin/dsh", args);
  }else {
    local.set_pid(pid);
    local.set_gpid(pid);
    local.set_running(true);
    processes_->insert(std::make_pair(local.name(), local));
    LOG(INFO, "create process %s successfully with pid %d", local.name().c_str(), local.pid());
  }
  return pid;
}

int32_t ProcessMgr::Clone(const Process& process, int flag) { 
  std::string cwd = process.cwd();
  bool mk_ok = MkdirRecur(cwd);
  if (!mk_ok) {
    LOG(WARNING, "fail create cwd dir %s", cwd.c_str());
    return -1;
  }
  std::string job_desc = cwd + "/"+  process.name() + "_process.yml";
  bool gen_ok = dsh_->GenYml(process,
                             job_desc);
  if (!gen_ok) {
    LOG(WARNING, "fail to gen process.yml for process %s", process.name().c_str());
    return -1;
  }
  CloneContext* context = new CloneContext();
  context->job_desc = job_desc;
  bool ok = GetOpenedFds(context->fds);
  if (!ok) {
    LOG(WARNING, "fail to get open fds for process %s", process.name().c_str());
    return -1;
  }
  int clone_ok = ::clone(&ProcessMgr::LaunchProcess,
                         CLONE_STACK + STACK_SIZE,
                         flag,
                         context);
  delete context;
  if (clone_ok == -1) {
    LOG(WARNING, "fail to clone process for process %s", process.name().c_str());
    return -1;
  }
  return clone_ok;
}

bool ProcessMgr::GetUser(const std::string& user,
                         int32_t* uid,
                         int32_t* gid) {
  if (user.empty() || uid == NULL || gid == NULL) {
    return false;
  }
  bool ok = false;
  struct passwd user_passd_info;
  struct passwd* user_passd_rs;
  char* user_passd_buf = NULL;
  int user_passd_buf_len = ::sysconf(_SC_GETPW_R_SIZE_MAX);
  for (int i = 0; i < 2; i++) {
    if (user_passd_buf != NULL) {
      delete []user_passd_buf; 
      user_passd_buf = NULL;
    }
    user_passd_buf = new char[user_passd_buf_len];
    int ret = ::getpwnam_r(user.c_str(), &user_passd_info, 
                user_passd_buf, user_passd_buf_len, &user_passd_rs);
    if (ret == 0 && user_passd_rs != NULL) {
      *uid = user_passd_rs->pw_uid; 
      *gid = user_passd_rs->pw_gid;
      ok = true;
      break;
    } else if (errno == ERANGE) {
      user_passd_buf_len *= 2; 
    }
  }
  if (user_passd_buf != NULL) {
    delete []user_passd_buf; 
    user_passd_buf = NULL;
  }
  return ok;
}

bool ProcessMgr::Wait(const std::string& name, Process* process) {
  std::map<std::string, Process>::iterator it = processes_->find(name);
  if (it == processes_->end()) {
    LOG(WARNING, "fail to find process with name %s", name.c_str());
    return false;
  }
  if (!it->second.running()) {
    process->CopyFrom(it->second);
    return true;
  }
  int status = -1;
  pid_t ret_pid = ::waitpid(it->second.pid(), &status, WNOHANG);
  // process exit
  if (ret_pid == it->second.pid()) {
    it->second.set_running(false);
    it->second.set_coredump(false);
    // normal exit
    if (WIFEXITED(status)) {
      it->second.set_exit_code(WEXITSTATUS(status));
    }else if (WIFSIGNALED(status)) {
      it->second.set_exit_code(128 + WTERMSIG(status));
      if (WCOREDUMP(status)) {
        it->second.set_coredump(true);
      }
    } 
    LOG(DEBUG, "process %s with pid %d exits with status %d", name.c_str(), 
        it->second.pid(), it->second.exit_code());
    process->CopyFrom(it->second);
  } else if (ret_pid == 0) {
    it->second.set_running(true);
    process->CopyFrom(it->second);
    LOG(DEBUG, "process %s with pid %d is running", name.c_str(), it->second.pid());
  } else {
    LOG(WARNING, "check process %s with pid %d with error %s ", name.c_str(), it->second.pid(), strerror(errno));
    return false;
  }
  return true;
}

bool ProcessMgr::Kill(const std::string& name, int signal) {
  std::map<std::string, Process>::iterator it = processes_->find(name);
  if (it == processes_->end()) {
    LOG(WARNING, "process with name %s does not exist", name.c_str());
    return false;
  }
  if (it->second.gpid() <= 0) {
    LOG(WARNING, "process with name %s has invalide gpid %d", 
        name.c_str(), 
        it->second.gpid());
    return false;
  }
  if (it->second.running()) {
    int ok = ::killpg(it->second.gpid(), signal);
    if (ok != 0) {
      LOG(WARNING, "fail to kill process group %d with err %s", it->second.gpid(), strerror(errno));
    }
  }
  LOG(INFO, "kill process with gid %d and signal %d successfully", it->second.gpid(), signal);
  processes_->erase(name);
  return true;
}

bool ProcessMgr::GetOpenedFds(std::set<int>& fds) {
  pid_t mypid = getpid();
  std::stringstream ss;
  ss << "/proc/" << mypid << "/fd/";
  std::string proc_path = ss.str();
  struct stat path_stat;
  int ret = ::stat(proc_path.c_str(), &path_stat);
  if (ret != 0) {
    LOG(WARNING, "fail to stat path %s for %s", 
                proc_path.c_str(),
                strerror(errno));
    return false;
  }
  if (S_ISDIR(path_stat.st_mode)) {
    DIR* dir = opendir(proc_path.c_str());
    if (dir == NULL) {
      LOG(WARNING, "fail to open path %s for %s", proc_path.c_str(),
                    strerror(errno));
      return false;
    }
    struct dirent *dirp;
    while ((dirp = readdir(dir)) != NULL) {
      std::string filename(dirp->d_name);
      if (filename.compare(".") == 0 
          || filename.compare("..") == 0) {
        continue;
      }
      int fd = boost::lexical_cast<int>(filename);
      if (fd == STDIN_FILENO
          || fd == STDOUT_FILENO
          || fd == STDERR_FILENO) {
        continue;
      }
      fds.insert(fd);
    }
    closedir(dir);
    return true;
  }
  return false;
}

}
