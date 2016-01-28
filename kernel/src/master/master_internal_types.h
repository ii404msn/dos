#ifndef KERNEL_MASTER_INTERNEL_TYPES_H
#define KERNEL_MASTER_INTERNEL_TYPES_H

namespace dos {

enum PodOperationType {
  kKillPod,
  kRunPod
};

struct PodOperation {
  PodOperationType type;
  PodStatus* status;
};

}

#endif
