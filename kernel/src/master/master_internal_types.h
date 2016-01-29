#ifndef KERNEL_MASTER_INTERNEL_TYPES_H
#define KERNEL_MASTER_INTERNEL_TYPES_H

#include "proto/dos.pb.h"

namespace dos {

enum PodOperationType {
  kKillPod,
  kRunPod
};

struct PodOperation {
  PodOperationType type_;
  PodStatus* pod_;
};

}

#endif
