#ifndef KERNEL_ENGINE_OCI_LOADER_H
#define KERNEL_ENGINE_OCI_LOADER_H

#include <string>
#include <stdint.h>
#include <vector>

namespace dos {
namespace oci {

struct User;
struct Platform;
struct Process;
struct Root;
struct Mount;

struct User {
  std::string name;
  int32_t uid;
  int32_t gid;
  User():name(),uid(0),gid(0){}
  ~User(){}
};

struct Platform {
  std::string os;
  std::string arch;
  Platform():os(), arch(){}
  ~Platform(){}
};

struct Process {
  bool terminal;
  User user;
  std::vector<std::string> args;
  std::vector<std::string> envs;
  std::string cwd;
  Process():terminal(false),
  user(), args(), envs(), cwd(){}
  ~Process(){}
};

struct Root {
  std::string path;
  bool readonly;
  Root():path(), readonly(true) {}
  ~Root(){}
};

struct Mount {
  std::string name;
  std::string path;
  Mount():name(),path(){}
  ~Mount(){}
};

struct Config {
  std::string version;
  std::string hostname;
  Platform platform;
  Process process;
  Root root;
  std::vector<Mount> mounts;
};

struct Runtime {};

bool LoadConfig(const std::string& path, Config* config);

} // namespace oci
} // namespace dos
#endif
