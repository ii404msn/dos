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
#include "engine/utils.h"

#define STACK_SIZE (1024 * 1024)
static char CLONE_STACK[STACK_SIZE];
const static int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
const static int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;
using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

int ProcessMgr::LaunchProcess(void* args) {
  CloneContext* context = reinterpret_cast<CloneContext*>(args);
  Dup2(context->stdout_fd, context->stderr_fd,context->stdin_fd);
  std::set<int>::iterator fd_it = context->fds.begin();
  for (; fd_it != context->fds.end(); ++fd_it) {
    int fd = *fd_it;
    if (fd == STDOUT_FILENO
        || fd == STDIN_FILENO
        || fd == STDERR_FILENO) {
      continue;
    }
    ::close(fd);
  }
/*  int set_hostname_ok = sethostname(context->process.name().c_str(),
                                      context->process.name().length());
  if (set_hostname_ok != 0) {
    fprintf(stderr, "fail to set hostname %s", 
            context->process.name().c_str());
    exit(1);
  }*/
  pid_t self_pid = ::getpid();
  int ret = ::setpgid(self_pid, self_pid);
  if (ret != 0) {
    assert(0);
  }
  if (!context->process.cwd().empty()) {
    ret = ::chdir(context->process.cwd().c_str());
    if (ret != 0) {
      assert(0);
    }
  }
  ret = ::setuid(context->process.user().uid());
  if (ret != 0) {
    assert(0);
  }
  ret = ::setgid(context->process.user().gid());
  if (ret != 0) {
    assert(0);
  }
  /*ret = ::setsid();
  if (ret != 0) {
    fprintf(stderr, "fail to setsid %s", strerror(errno));
    assert(0);
  }*/
  char* argv[context->process.args_size() + 1];
  int32_t argv_index = 0;
  for (;argv_index < context->process.args_size(); ++argv_index) {
    argv[argv_index] = const_cast<char*>(context->process.args(argv_index).c_str());
  }
  argv[argv_index + 1] = NULL;
  ::execv(argv[0], argv);
  assert(0);
}

ProcessMgr::ProcessMgr():processes_(NULL){
  processes_ = new std::map<std::string, Process>();
}

ProcessMgr::~ProcessMgr(){}

bool ProcessMgr::Exec(const Process& process) {
  Process local;
  local.CopyFrom(process);
  local.set_rtime(::baidu::common::timer::get_micros());
  if (local.user().uid() < 0) {
    LOG(WARNING, "user is required");
    return false;
  }
  if (local.cwd().empty()) {
    local.set_cwd("/home/" + process.user().name());
  }
  bool ok = MkdirRecur(local.cwd());
  if (!ok) {
    LOG(WARNING, "fail create workdir %s", local.cwd().c_str());
    return false;
  }
  int stdout_fd = -1;
  int stderr_fd = -1;
  int stdin_fd = -1;
  ok = ResetIo(local, stdout_fd, stderr_fd, stdin_fd);
  if (!ok) {
    LOG(WARNING, "fail to create stdout stderr descriptor for process %s", local.name().c_str());
    return false;
  }
  std::set<int> openfds;
  ok = GetOpenedFds(openfds);
  if (!ok) {
    LOG(WARNING, "fail to get opened fds  ");
    return false;
  }
  pid_t pid = fork();
  if (pid == -1) {
    LOG(WARNING, "fail to fork process %s", local.name().c_str());
    return false;
  }else if (pid == 0) {
    std::set<int>::iterator fd_it = openfds.begin();
    Dup2(stdout_fd, stderr_fd, stdin_fd);
    for (; fd_it !=  openfds.end(); ++fd_it) {
      int fd = *fd_it;
      if (fd == STDOUT_FILENO
          || fd == STDIN_FILENO
          || fd == STDERR_FILENO) {
        continue;
      }
      close(fd);
    } 
    pid_t self_pid = getpid();
    int ret = setpgid(self_pid, self_pid);
    if (ret != 0) {
      assert(0);
    }
    if (!local.cwd().empty()) { 
      ret = chdir(local.cwd().c_str());
      if (ret != 0) {
        assert(0);
      }
    } 
    ret = setuid(local.user().uid());
    if (ret != 0) {
      assert(0);
    }
    ret = setgid(local.user().gid());
    if (ret != 0) {
      assert(0);
    }
    if (!local.pty().empty()) {
      /*pid_t sid = setsid();
      if (sid == -1) {
        assert(0);
      }*/
    }
    if (local.use_bash_interceptor()) {
      std::string cmd;
      for (int32_t index = 0; index < local.args_size(); ++index) {
        if (index > 0) {
          cmd += " ";
        }
        cmd += local.args(index);
      }
      printf("use bash to exec cmd %s", cmd.c_str());
      char* argv[] = {
              const_cast<char*>("bash"),
              const_cast<char*>("-c"),
              const_cast<char*>(cmd.c_str()),
              NULL};
      char* env[local.envs_size() + 1];
      int32_t env_index = 0;
      for (; env_index < local.envs_size(); env_index ++) { 
        env[env_index] =const_cast<char*>(local.envs(env_index).c_str());
      }
      env[env_index+1] = NULL;
      ::execve("/bin/bash", argv, env);
      assert(0);
    } else {
      char* argv[local.args_size() + 1];
      std::string cmd;
      for (int32_t index = 0; index < local.args_size(); ++index) {
        argv[index] = const_cast<char*>(local.args(index).c_str());
      }
      printf("use self to exec cmd %s", cmd.c_str());
      argv[local.args_size()] = NULL;
      char* env[local.envs_size() + 1];
      int32_t env_index = 0;
      for (; env_index < local.envs_size(); env_index ++) { 
        env[env_index] = const_cast<char*>(local.envs(env_index).c_str());
      }
      env[env_index+1] = NULL;
      ::execve(argv[0], argv, env);
      assert(0);
    }
  }else {
    if (stdout_fd != -1) {
      ::close(stdout_fd);
    }
    if (stderr_fd != -1) {
      ::close(stderr_fd);
    }
    if (stdin_fd != -1) {
      ::close(stdin_fd);
    }
    local.set_pid(pid);
    local.set_gpid(pid);
    local.set_running(true);
    processes_->insert(std::make_pair(local.name(), local));
    LOG(INFO, "create process %s successfully with pid %d", local.name().c_str(), local.pid());
    return true;
  }
}

bool ProcessMgr::Clone(const Process& process, int flag) { 
  CloneContext* context = new CloneContext();
  context->process = process;
  context->stdout_fd = -1;
  context->stderr_fd = -1;
  context->stdin_fd = -1;
  context->flags = flag;
  bool ok = ResetIo(process, context->stdout_fd,
                    context->stderr_fd, 
                    context->stdin_fd);
  if (!ok) {
    LOG(WARNING, "fail to reset io for process %s", process.name().c_str());
    return false;
  }
  ok = GetOpenedFds(context->fds);
  if (!ok) {
    LOG(WARNING, "fail to get open fds for process %s", process.name().c_str());
    return false;
  }
  int clone_ok = ::clone(&ProcessMgr::LaunchProcess,
                         CLONE_STACK + STACK_SIZE,
                         flag,
                         context);
  if (context->stdout_fd != -1) {
    close(context->stdout_fd);
  }
  if (context->stderr_fd != -1) {
    close(context->stderr_fd);
  }
  if (context->stdin_fd != -1) {
    close(context->stdin_fd);
  }
  delete context;
  if (clone_ok == -1) {
    LOG(WARNING, "fail to clone process for process %s", process.name().c_str());
    return false;
  }
  return true;
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

bool ProcessMgr::ResetIo(const Process& process,
                         int& stdout_fd,
                         int& stderr_fd,
                         int& stdin_fd) { 
  if (process.pty().empty()) {
    std::string stdout_path = process.cwd() + "/stdout";
    std::string stderr_path = process.cwd() + "/stderr";
    stdout_fd = open(stdout_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (stdout_fd == -1) {
        LOG(WARNING, "fail to create %s for err %s", stdout_path.c_str(), strerror(errno));
        return false;
    }
    stderr_fd = open(stderr_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (stderr_fd == -1) {
        LOG(WARNING, "fail to create %s for err %s", stderr_path.c_str(), strerror(errno));
    }
    LOG(INFO,"create std file in %s  with stdout %d stderr %d", 
        process.cwd().c_str(),
        stdout_fd,
        stderr_fd);
  }else {
    int pty_fd = open(process.pty().c_str(), O_RDWR);
    stdout_fd = pty_fd;
    stderr_fd = pty_fd;
    stdin_fd = pty_fd;
  } 
  return true;
}

void ProcessMgr::Dup2(const int stdout_fd,
                      const int stderr_fd,
                      const int stdin_fd) {
  while (stdout_fd != -1 
         && ::dup2(stdout_fd, STDOUT_FILENO) == -1
         && errno == EINTR) {}
  while (stderr_fd != -1 
         && ::dup2(stderr_fd, STDERR_FILENO) == -1
         && errno == EINTR) {} 
  while (stdin_fd != -1
         &&::dup2(stdin_fd, STDIN_FILENO) == -1
         && errno == EINTR) {}
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
