#ifndef KERNEL_DSH_DSH_H
#define KERNEL_DSH_DSH_H

#include "yaml-cpp/yaml.h"
#include "proto/dos.pb.h"

namespace dos {

class Dsh {

public:
  Dsh();
  ~Dsh();
  // generate a process configuration with yaml format
  // if is_leader equals true, setsid is invoked 
  bool GenYml(const Process& process,
              const std::string& path,
              bool is_leader);

  // load yaml from local disk , the path
  // is abusolutly path
  bool LoadAndRunByYml(const std::string& path);
private:
  bool PrepareStdio(const YAML::Node& config);
  bool PrepareUser(const YAML::Node& config);
  void Exec(const YAML::Node& config);
};

} // namespace dos
#endif
