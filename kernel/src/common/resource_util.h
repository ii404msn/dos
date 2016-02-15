#ifndef KERNEL_COMMON_RESOURCE_UTIL_H
#define KERNEL_COMMON_RESOURCE_UTIL_H

#include <set>

#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::DEBUG;
using ::baidu::common::WARNING;

namespace dos {

class ResourceUtil {

public:
  ResourceUtil(){}
  ~ResourceUtil(){}

  static bool Plus(const Resource& add, Resource* target) {
    if (target == NULL) {
      return false;
    }
    target->mutable_cpu()->set_limit(target->cpu().limit() + add.cpu().limit());

    target->mutable_memory()->set_limit(target->memory().limit() + add.memory().limit());

    for (int32_t index = 0; index < add.port().assigned_size(); ++index ) {
      target->mutable_port()->add_assigned(add.port().assigned(index));
    }
    return true;
  }

  // release resource to target 
  // currently support cpu memory and ports
  // this function only modifies assigned property
  static bool Release(const Resource& alloc, Resource* target) {
    if (target == NULL) {
      return false;
    }
    target->mutable_cpu()->set_assigned(target->cpu().assigned() - alloc.cpu().limit());
    target->mutable_memory()->set_assigned(target->memory().assigned() - alloc.memory().limit());
    // handle process ports
    std::set<uint32_t> assigned_ports;
    for (int32_t index = 0; index < target->port().assigned_size(); ++index) {
      assigned_ports.insert(target->port().assigned(index));
    }
    // clear old assigned ports
    target->mutable_port()->clear_assigned();
    for (int32_t index = 0; index < alloc.port().assigned_size(); ++index) {
      // the ports that pod used are assigned property
      assigned_ports.erase(alloc.port().assigned(index));
    }
    std::set<uint32_t>::iterator port_it = assigned_ports.begin();
    for (; port_it != assigned_ports.end(); ++port_it) {
      target->mutable_port()->add_assigned(*port_it);
    }
    return true;
  }

  // check left is satisfy resource requirement
  // use left (limit - assigned) to compare right limit
  // currently support cpu memory and ports
  static bool Satisfy(const Resource* left, const Resource* right) {
    // compare cpu 
    uint64_t cpu_left = left->cpu().limit() - left->cpu().assigned();
    if (cpu_left < right->cpu().limit()) {
      LOG(DEBUG, "left cpu %ld is little than  right %ld",
          cpu_left, right->cpu().limit());
      return false;
    }
    // compare memory
    uint64_t mem_left = left->memory().limit() - left->memory().assigned();
    if (mem_left < right->memory().limit()) {
      LOG(DEBUG, "left memory %ld is little than  right %ld",
          mem_left, right->memory().limit());
      return false;
    }
    // compare ports, use right port assigned property to compare
    // left (range - assigned)
    std::set<uint32_t> assigned_ports;
    for (int32_t index = 0; index < left->port().assigned_size(); ++index) {
      assigned_ports.insert(left->port().assigned(index));
    }
    for (int32_t index = 0; index < right->port().assigned_size(); ++index) {
      uint32_t port = right->port().assigned(index);
      if (port < left->port().range().start() 
          || port > left->port().range().end()) {
        LOG(DEBUG, "the require port %d is not in range[%d, %d]",
            port, left->port().range().start(),
            left->port().range().end());
        return false;
      }
      if (assigned_ports.find(port) != assigned_ports.end()) {
        LOG(DEBUG, "the port %d has been assigned", port);
        return false;
      }
    }
    return true;
  }

  // alloc resouce from target
  // currently support cpu memory and ports
  // this function only modifies assigned property
  static bool Alloc(const Resource& sub, Resource* target) {
    if (target == NULL) {
      return false;
    }
    if (!Satisfy(target, &sub)) {
      return false;
    }

    // alloc cpu
    target->mutable_cpu()->set_assigned(target->cpu().assigned() + sub.cpu().limit());

    // alloc memory
    target->mutable_memory()->set_assigned(target->memory().assigned() +  sub.memory().limit());

    // alloc port
    std::set<uint32_t> assigned_ports;
    for (int32_t index = 0; index < target->port().assigned_size(); ++index) {
      assigned_ports.insert(target->port().assigned(index));
    }
    target->mutable_port()->clear_assigned();
    for (int32_t index = 0; index < sub.port().assigned_size(); ++index) {
      LOG(DEBUG, "alloc port %d", sub.port().assigned(index));
      assigned_ports.insert(sub.port().assigned(index));
    }
    std::set<uint32_t>::iterator port_it = assigned_ports.begin();
    for (; port_it != assigned_ports.end(); ++port_it) {
      target->mutable_port()->add_assigned(*port_it);
    }
    return true;
  }
};

}

#endif
