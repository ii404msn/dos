// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef KERNEL_ENGINE_OC_H
#define KERNEL_ENGINE_OC_H

#include "proto/initd.pb.h"

#include <string>
#include <map>
#include <set>

namespace dos {

class Oc {
public:
  Oc(const std::string& oc_path,
     const std::string& runtime_config);
  ~Oc();
  bool Init();
private:
  // init default mount like proc sys devices
  bool LoadRuntime();
  bool DoMount(const std::string& destination,
               const std::string& type);
  bool DoMknod(const std::string& rootfs);
  bool DoBind(const std::string& from, 
              const std::string& to);
  // init custome image rootfs 
  bool InitImageRootfs();
  // init system cgroup path, this should be invoked before InitImageRootfs
  bool InitCgroup();
private:
  std::string oc_path_;
  std::string runtime_config_;
  // type mount pair
  std::map<std::string, Mount* > mounts_;
  // path device pair
  std::map<std::string, Device* > devices_;
  std::set<std::string> namespaces_;
};

}
#endif
