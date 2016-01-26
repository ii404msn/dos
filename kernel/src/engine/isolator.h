#ifndef KERNEL_ENGINE_ISOLATOR_H
#define KERNEL_ENGINE_ISOLATOR_H
namespace dos {

// cpu isolator implemented by cgroup
class CpuIsolator {

};

// memory isolator implemented by ulimit
class MemoryIsolator {

};

// device io isolator implemented by cgroup
// 
class DeviceIoIsolator {};

// network io isolator implemented by cgroup
class NetworkIoIsolator {};

} // namespace dos

#endif
