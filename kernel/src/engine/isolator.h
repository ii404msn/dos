#ifndef KERNEL_ENGINE_ISOLATOR_H
#define KERNEL_ENGINE_ISOLATOR_H
namespace dos {

// cpu isolator implemented by cgroup
class CpuIsolator {

public:
  CpuIsolator(const std::string& path);
  ~CpuIsolator();
  // attach pid this isolator
  bool Attach(int32_t pid);
  bool AssignQuota(int32_t quota);
  bool AssignLimit(int32_t limit);

private:
  std::string path_;
  int32_t quota_;
  int32_t limit_;
};

// memory isolator implemented by cgroup
class MemoryIsolator {

};

// device io isolator implemented by cgroup
// 
class DeviceIoIsolator {};

// network io isolator implemented by cgroup
class NetworkIoIsolator {};

} // namespace dos

#endif
