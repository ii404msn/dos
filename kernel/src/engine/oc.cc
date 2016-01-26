// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "engine/oc.h"

#include <sys/mount.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <unistd.h>
#include <errno.h>
#include "engine/utils.h"
#include "engine/oci_loader.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

const static std::string DEFAULT_MOUNT_OPTION = "";
const static uint64_t DEFAULT_MOUNT_FLAGS = 0;

Oc::Oc(const std::string& oc_path,
       const std::string& runtime_config):oc_path_(oc_path),
  runtime_config_(runtime_config){}

Oc::~Oc(){}

bool Oc::LoadRuntime() {
  FILE* fd = fopen(runtime_config_.c_str(), "r");
  if (fd == NULL) {
      LOG(WARNING, "fail to open %s ", runtime_config_.c_str());
      return false;
  }
  char buffer[5120];
  rapidjson::FileReadStream frs(fd, buffer, sizeof(buffer));
  rapidjson::Document document;
  if (document.ParseStream<0>(frs).HasParseError()
      || !document.IsObject()) {
      fclose(fd);
      LOG(WARNING, "fail to parse runtime_config %s", runtime_config_.c_str());
      return false;
  }
  fclose(fd);
  if (!document.HasMember("mounts")) {
      LOG(WARNING, "mounts is requred");
      return false;
  }
  const rapidjson::Value& mounts = document["mounts"];
  rapidjson::Value::ConstMemberIterator it = mounts.MemberBegin();
  for (; it != mounts.MemberEnd(); ++it) {
    const rapidjson::Value& mount = it->value;
    Mount* mnt = new Mount();
    mnt->set_type(mount["type"].GetString());
    mnt->set_source(mount["source"].GetString());
    const rapidjson::Value& options = mount["options"];
    if (options.IsObject()) {
      for (rapidjson::SizeType i = 0; i < options.Size(); i++) {
        std::string option = options[i].GetString();
        mnt->add_options(option);
      }
    }
    LOG(DEBUG, "load mount %s config successully", mnt->source().c_str());
    mounts_.insert(std::make_pair(it->name.GetString(), mnt));
  }
  if (!document.HasMember("linux")) {
    LOG(WARNING, "linux is required");
    return false;
  }
  const rapidjson::Value& linux_config = document["linux"];
  if (!linux_config.HasMember("namespaces")) {
    LOG(WARNING, "namespace is required");
    return false;
  }
  const rapidjson::Value& namespaces = linux_config["namespaces"];
  for (rapidjson::SizeType i = 0; i < namespaces.Size(); i++) {
    const rapidjson::Value& ns = namespaces[i];
    namespaces_.insert(ns["type"].GetString());
  }
  if (!linux_config.HasMember("devices")) {
    LOG(WARNING, "devices is required");
    return false;
  }
  const rapidjson::Value& devices = linux_config["devices"];
  for (rapidjson::SizeType i = 0; i < devices.Size(); i++) {
    const rapidjson::Value& device = devices[i];
    Device* dev = new Device();
    dev->set_path(device["path"].GetString());
    dev->set_type(device["type"].GetInt());
    dev->set_major(device["major"].GetInt());
    dev->set_minor(device["minor"].GetInt());
    dev->set_permissions(device["permissions"].GetString());
    dev->set_mode(device["fileMode"].GetInt());
    dev->set_uid(device["uid"].GetInt());
    dev->set_gid(device["gid"].GetInt());
    devices_.insert(std::make_pair(dev->path(), dev));
  }
  LOG(INFO, "load runtime config successully");
  return true;
}


bool Oc::Init() {
  bool load_ok = LoadRuntime();
  if (!load_ok) {
    LOG(WARNING, "fail to load runtime config");
    return false;
  }
  int ok = chdir(oc_path_.c_str());
  if (ok != 0) {
    LOG(WARNING, "fail to chdir to %s", oc_path_.c_str());
    return false;
  }
  ::dos::Config config;
  bool load_ok = ::dos::LoadConfig("config.json", &config);
  if (!load_ok) {
    LOG(WARNING, "fail to load config.json");
  }
  std::string rootfs_path = config.root.path();
  std::vector< ::dos::Mount>::iterator m_it = config.mounts.begin();
  for (; m_it != config.mounts.end(); ++m_it) {
    bool ok = DoMount(rootfs_path + m_it->path(), m_it->name());
    if (!ok) {
      return false;
    }
  }
  LOG(INFO, "init custom mounts successully");
  bool device_ok = DoMknod(rootfs_path);
  if (device_ok) {
    ok = chroot(rootfs_path.c_str());
    if (ok !=0) {
      LOG(WARNING, "fail to chroot to %s",rootfs_path.c_str());
      return false;
    }
    LOG(INFO, "change root successully");
  }
  return device_ok;
}

bool Oc::DoMount(const std::string& destination,
                 const std::string& type) {
  std::map<std::string, Mount* >::iterator it = mounts_.find(type);
  if (it == mounts_.end()) {
    LOG(WARNING, "mount %s has no runtime config", type.c_str());
    return false;
  }
  Mount* config = it->second;
  std::string options;
  for (int i = 0; i < config->options_size(); i++) {
    if (i != 0) {
      options += ",";
    }
    options += config->options(i);
  }
  bool ret = Mkdir(destination);
  if (!ret) {
    return false;
  }
  int ok = ::mount(config->source().c_str(), 
                   destination.c_str(), 
                   config->type().c_str(),
                   0, 
                   options.c_str());
  if (ok != 0) {
    LOG(WARNING, "fail to mount %s to %s with type %s flags %ld, options %s, err %s",
                config->source().c_str(),
                destination.c_str(), 
                type.c_str(), 0, 
                options.c_str(),
                strerror(errno));
    return false;
  }
  LOG(INFO, "mount %s successully", type.c_str());
  return true;
}

bool Oc::DoMknod(const std::string& rootfs) {
  std::map<std::string, Device* >::iterator it = devices_.begin();
  for (; it != devices_.end(); ++it) {
    Device* config = it->second;
    dev_t dt = makedev(config->major(), config->minor());
    std::string path = rootfs + it->first;
    int ok = mknod(path.c_str(), config->mode(), dt);
    if (ok != 0) {
      LOG(WARNING, "fail make a device to %s with err %s", it->first.c_str(),
                strerror(errno));
      return false; 
    }
    LOG(DEBUG, "create device %s successully", it->first.c_str());
  }
  LOG(INFO, "create devices successully");
  return true;
}

}
