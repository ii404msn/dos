#ifndef KERNEL_ENGINE_OCI_LOADER_H
#define KERNEL_ENGINE_OCI_LOADER_H

#include <string>
#include <stdint.h>
#include <vector>
#include "proto/dos.pb.h"

namespace dos {

struct Config {
  std::string version;
  std::string hostname;
  Platform platform;
  Process process;
  Root root;
  std::vector<Mount> mounts;
};


bool LoadConfig(const std::string& path, Config* config);

} // namespace dos
#endif
