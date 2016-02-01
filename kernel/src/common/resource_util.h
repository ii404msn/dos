#ifndef KERNEL_COMMON_RESOURCE_UTIL_H
#define KERNEL_COMMON_RESOURCE_UTIL_H

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
    target->mutable_cpu()->set_share(target->cpu().share() + add.cpu().share());
    target->mutable_cpu()->set_used(target->cpu().used() + add.cpu().used());

    target->mutable_memory()->set_limit(target->memory().limit() + add.memory().limit());
    target->mutable_memory()->set_used(target->memory().used() + add.memory().used());

    //TODO port
  }
};

}

#endif
