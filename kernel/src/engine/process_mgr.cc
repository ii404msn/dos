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
#include <sstream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <assert.h>
#include <errno.h>
#include "logging.h"
#include "timer.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

ProcessMgr::ProcessMgr():processes_(NULL){
  processes_ = new std::map<std::string, Process>();
  mypid_ = ::getpid();
}

ProcessMgr::~ProcessMgr(){}

bool ProcessMgr::Exec(const Process& process) {
  int32_t uid;
  int32_t gid;
  bool ok = GetUser(process.user().name(), &uid, &gid);
  if (!ok) {
    LOG(WARNING, "user %s does not exists", process.user().name().c_str());
    return false;
  }
  Process local;
  local.CopyFrom(process);
  local.set_rtime(::baidu::common::timer::get_micros());
  std::set<int> openfds;
  ok = GetOpenedFds(openfds);
  if (!ok) {
    LOG(WARNING, "fail to get opened fds of pid %d ", mypid_);
    return false;
  }
  pid_t pid = fork();
  if (pid == -1) {
    LOG(WARNING, "fail to fork process %s", local.name().c_str());
    return false;
  }else if (pid == 0) {
    std::set<int>::iterator fd_it = openfds.begin();
    for (; fd_it !=  openfds.end(); ++fd_it) {
      close(*fd_it);
    }
    ok = ResetIo(local);
    if(!ok) {
      assert(0);
    }
    pid_t self_pid = getpid();
    int ret = setpgid(self_pid, self_pid);
    if (ret != 0) {
      fprintf(stderr, "fail to set pgid %d", self_pid);
      assert(0);
    }
    if (!local.cwd().empty()) {
      ret = chdir(local.cwd().c_str());
      if (ret != 0) {
        fprintf(stderr, "fail to chdir to %s", local.cwd().c_str());
        assert(0);
      }
    } 
    ret = setuid(uid);
    if (ret != 0) {
      fprintf(stderr, "fail to set uid %d", uid);
      assert(0);
    }
    ret = setgid(gid);
    if (ret != 0) {
      fprintf(stderr, "fail to set gid %d", gid);
      assert(0);
    }
    std::string cmd;
    for (int32_t index = 0; index < local.args_size(); ++index) {
      if (index > 0) {
        cmd += " ";
      }
      cmd += local.args(index);
    }
    char* argv[] = {
            const_cast<char*>("sh"),
            const_cast<char*>("-c"),
            const_cast<char*>(cmd.c_str()),
            NULL};
    char* env[local.envs_size() + 1];
    int32_t env_index = 0;
    for (; env_index < local.envs_size(); env_index ++) { 
      env[env_index] =const_cast<char*>(local.envs(env_index).c_str());
    }
    env[env_index+1] = NULL;
    ::execve("/bin/sh", argv, env);
    assert(0);
  }else {
    local.set_pid(pid);
    local.set_gpid(pid);
    local.set_running(true);
    processes_->insert(std::make_pair(local.name(), local));
    LOG(INFO, "create process %s successfully with pid %d", local.name().c_str(), local.pid());
    return true;
  }
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
        break;
  }
  if (user_passd_buf != NULL) {
      delete []user_passd_buf; 
      user_passd_buf = NULL;
  }
  return ok;
}

bool ProcessMgr::ResetIo(const Process& process) {
  int stdout_fd = -1;
  int stderr_fd = -1;
  int stdin_fd = -1;
  if (process.pty().empty()) {
    stdout_fd = open("./stdout", O_WRONLY);
    stderr_fd = open("./stderr", O_WRONLY);
  }else {
    int pty_fd = open(process.pty().c_str(), O_RDWR);
    stdout_fd = pty_fd;
    stderr_fd = pty_fd;
    stdin_fd = pty_fd;
  }
  bool ret = true;
  do {
    int ok = 0;
    if (stdout_fd != -1) {
      ok = dup2(stdout_fd, STDOUT_FILENO);
      if (ok == -1) {
        ret = false;
        break;
      }
    }
    if (stdin_fd != -1) {
      ok = dup2(stdin_fd, STDIN_FILENO);
      if (ok == -1) {
        ret = false;
        break;
      }
    }
    if (stderr_fd != -1) {
      ok = dup2(stderr_fd, STDERR_FILENO);
      if (ok == -1) {
        ret = false;
        break;
      }
    }
  } while(0);
  close(stdout_fd);
  close(stdin_fd);
  close(stderr_fd);
  return ret;
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
    LOG(DEBUG, "process %s with pid %d exists", name.c_str(), it->second.pid());
    it->second.set_running(false);
    it->second.set_coredump(false);
    // normal exit
    if (WIFEXITED(status)) {
      it->second.set_exit_code(WEXITSTATUS(status));
    }else if(WCOREDUMP(status)) {
      it->second.set_coredump(true);
      it->second.set_exit_code(9);
    }
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
  int ok = ::killpg(it->second.gpid(), signal);
  processes_->erase(name);
  if (ok != 0) {
    LOG(WARNING, "fail to kill process group %d with err %s", it->second.gpid(), strerror(errno));
    return false;
  }
  LOG(INFO, "kill process with gid %d and signal %d successfully", it->second.gpid(), signal);
  return true;
}

bool ProcessMgr::GetOpenedFds(std::set<int>& fds) {
  std::stringstream ss;
  ss << "/proc/" << mypid_ << "/fd/";
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
