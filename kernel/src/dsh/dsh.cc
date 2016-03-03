#include "dsh/dsh.h"

#include <fstream> 
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

const static int STD_FILE_OPEN_FLAG = O_CREAT | O_APPEND | O_WRONLY;
const static int STD_FILE_OPEN_MODE = S_IRWXU | S_IRWXG | S_IROTH;

Dsh::Dsh() {}
Dsh::~Dsh () {}

bool Dsh::GenYml(const Process& process, 
                 const std::string& path) {
  YAML::Node node;
  // name is a required field
  if (process.name().empty()) {
    LOG(WARNING, "name is empty");
    return false;
  }
  node["name"] = process.name();
  node["hostname"] = process.hostname();
  node["uid"] = process.user().uid(); 
  node["gid"] = process.user().gid(); 
  // cwd is a required field
  if (process.cwd().empty()) {
    LOG(WARNING, "cwd is empty");
    return false;
  }
  node["cwd"] = process.cwd();
  // interceptor is a required 
  if (process.interceptor().empty()) {
    LOG(WARNING, "interceptor is required");
    return false;
  }
  node["interceptor"] = process.interceptor();
  for (int32_t index = 0; index < process.args_size(); ++index) {
    node["args"].push_back(process.args(index));
  }
  for (int32_t index = 0; index < process.envs_size(); ++index) {
    node["envs"].push_back(process.envs(index));
  }
  node["pty"] = process.pty();
  std::ofstream out(path.c_str());
  out << node;
  return true;
}


void Dsh::LoadAndRunByYml(const std::string& path) {
  YAML::Node config = YAML::LoadFile(path);
  if (!config["name"]) {
    LOG(WARNING, "name is required in config %s", path.c_str());
    exit(-1);
  }
  std::string name = config["name"].as<std::string>();
  bool io_ok = PrepareStdio(config);
  if (!io_ok) {
    LOG(WARNING, "fail to prepare stdio for process %s with config %s",
        name.c_str(),
        path.c_str());
    exit(-1);
  }
  LOG(INFO, "start process %s with config %s", name.c_str(), path.c_str());
  bool user_ok = PrepareUser(config);
  if (!user_ok) {
    LOG(WARNING, "fail to prepare user for process %s with config %s",
        name.c_str(),
        path.c_str());
    exit(-1);
  }
  Exec(config);
  LOG(WARNING, "process %s exit", name.c_str());
  exit(-1);
}

bool Dsh::PrepareUser(const YAML::Node& config) {
  std::string name = config["name"].as<std::string>();
  LOG(INFO, "set use config for process %s", name.c_str());
  if (!config["uid"]) {
    LOG(WARNING, "uid is required for process %s", name.c_str());
    return false;
  }

  if (!config["gid"]) {
    LOG(WARNING, "gid is required for process %s", name.c_str());
    return false;
  }

  int ret = ::setuid(config["uid"].as<int32_t>());
  if (ret != 0) {
    LOG(WARNING, "fail to set uid %d for process %s with err %s",
        config["uid"].as<int32_t>(), name.c_str(),
        strerror(errno));
    return false;
  }

  ret = ::setgid(config["gid"].as<int32_t>());
  if (ret != 0) {
    LOG(WARNING, "fail to set gid %d for process %s with err %s",
        config["gid"].as<int32_t>(), name.c_str(),
        strerror(errno));
    return false;
  }
  if (config["hostname"]) {
    std::string hostname = config["hostname"].as<std::string>();
    if (!hostname.empty()) {
      int set_hostname_ok = sethostname(hostname.c_str(),
                                        hostname.length());
      if (set_hostname_ok != 0) {
        LOG(WARNING, "fail to set hostname %s for process %s with err %s",
           hostname.c_str(),
           name.c_str(),
           strerror(errno));
        return false;
      }
    }
  }
  return true;
}

// redirect stdout, stderr to files 
// or to pty for interactive process
bool Dsh::PrepareStdio(const YAML::Node& config) {
  std::string name = config["name"].as<std::string>();
  std::string pty;
  if (config["pty"]) {
    pty = config["pty"].as<std::string>();
  } 
  // TODO use dproc to store stdout and stderr
  std::string cwd = config["cwd"].as<std::string>();
  int chok = chdir(cwd.c_str());
  if (chok != 0) {
    LOG(WARNING, "fail to change dir to %s", cwd.c_str());
    return false;
  }
  int stdout_fd = -1;
  int stderr_fd = -1;
  int stdin_fd = -1;
  // TODO close fd when return false
  if (pty.empty()) {
    LOG(INFO, "create stdio with stdout and stderr files for process %s in dir %s",
        name.c_str(),
        cwd.c_str());
    std::string stdout_path = cwd + "/stdout";
    std::string stderr_path = cwd + "/stderr";
    stdout_fd = open(stdout_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (stdout_fd == -1) {
        LOG(WARNING, "fail to create %s for process %s with err %s", 
            stdout_path.c_str(),
            name.c_str(),
            strerror(errno));
        return false;
    }
    stderr_fd = open(stderr_path.c_str(), STD_FILE_OPEN_FLAG, STD_FILE_OPEN_MODE);
    if (stderr_fd == -1) {
        LOG(WARNING, "fail to create %s for process %s with err %s",
            stderr_path.c_str(),
            name.c_str(),
            strerror(errno));
        return false;
    }
    LOG(INFO,"create std file in %s  with stdout %d stderr %d", 
        cwd.c_str(),
        stdout_fd,
        stderr_fd);
  } else {
    int pty_fd = open(pty.c_str(), O_RDWR);
    stdout_fd = pty_fd;
    stderr_fd = pty_fd;
    stdin_fd = pty_fd;
    pid_t sid = setsid();
    if (sid == -1) {
      LOG(WARNING, "fail to set sid for process %s with err %s",
          name.c_str(), strerror(errno));
      return false;
    }
    ioctl(0, TIOCSCTTY, 1);
  }

  if (stdout_fd != -1) {
    close(STDOUT_FILENO);
    int ret = dup2(stdout_fd, STDOUT_FILENO);
    if (ret == -1) {
      LOG(WARNING, "fail to dup %d to stdout for %s",
          stdout_fd,
          strerror(errno));
      return false;
    }
  }

  if (stderr_fd != -1) {
    close(STDERR_FILENO);
    int ret = dup2(stderr_fd, STDERR_FILENO);
    if (ret == -1) {
      LOG(WARNING, "fail to dup %d to stderr for %s",
          stderr_fd,
          strerror(errno));
      return false;
    }
  }
  
  if (stdin_fd != -1) {
    close(STDIN_FILENO);
    int ret = dup2(stdin_fd, STDIN_FILENO);
    if (ret == -1) {
      LOG(WARNING, "fail to dup %d to stdin for %s",
          stdin_fd,
          strerror(errno));
      return false;
    }
  }
  LOG(INFO, "prepare stdio for process %s successfully", name.c_str());
  return true;
}


void Dsh::Exec(const YAML::Node& config) {
  std::string name = config["name"].as<std::string>();
  if (!config["args"]) {
    LOG(WARNING, "args is required for process %s", name.c_str());  
    return; 
  }
  if (!config["interceptor"]) {
    LOG(WARNING, "interceptor is required for process %s", name.c_str());
    return;
  }
  std::string log = "start process with comand [ ";
  std::string interceptor = config["interceptor"].as<std::string>();
  char* args[config["args"].size() + 1];
  for (uint32_t index = 0; index < config["args"].size(); ++index) {
    args[index] = const_cast<char*>(config["args"][index].as<std::string>().c_str());
    log += config["args"][index].as<std::string>() + " ";
  }
  log += "] with envs [ ";
  args[config["args"].size()] = NULL; 
  uint32_t envs_size = 0;
  if (config["envs"]) {
    envs_size = config["envs"].size();
  }
  char* envs[envs_size + 1];
  if (config["envs"]) {
    for (uint32_t index = 0; index < config["envs"].size(); ++index) {
      envs[index] = const_cast<char*>(config["envs"][index].as<std::string>().c_str());
      log += config["envs"][index].as<std::string>() + " ";
    }
  } 
  log += "]";
  LOG(INFO, "%s", log.c_str());
  envs[envs_size] = NULL;
  ::execve(interceptor.c_str(), args, envs);
}

} // namespace dos
