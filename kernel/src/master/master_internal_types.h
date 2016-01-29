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

enum JobOperationType {
  kJobNewAdd,
  kJobRemove,
  kJobUpdate
};

struct JobOperation {
  JobStatus* job_;
  JobOperationType type_;
};

}

#endif
