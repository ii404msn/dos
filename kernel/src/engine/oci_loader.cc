#include "engine/oci_loader.h"

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "logging.h"

namespace dos {
namespace oci {

bool LoadConfig(const std::string& path, Config* config) {
  if (config == NULL) {
    return false;
  }
  FILE* fd = fopen(path.c_str(), "r");
  if (fd == NULL) {
    LOG(WARNING, "fail to open %s", path.c_str());
    return false;
  }
  char buffer[5120];
  rapidjson::FileReadStream frs(fd, buffer, sizeof(buffer));
  rapidjson::Document document;
  if (document.ParseStream<0>(frs).HasParseError()) {
    LOG(WARNING, "fail to parse rootfs config.json in %s", oc_path_.c_str());
    fclose(fd);
    return false;
  }
  fclose(fd);
  if (document.HasMember("version")) {
    config->version = document["version"].GetString();
  } else {
    config->version = "unknown";
  }
  if (document.HasMember("hostname")) {
    config->hostname = document["hostname"].GetString();
  } else {
    config->hostname = "dos";
  }
  if (!document.HasMember("root")) {
    LOG(WARNING, "fail to root in config.json");
    return false;
  }
  const rapidjson::Value& root = document["root"];
  if (!root.HasMember("path")) {
    LOG(WARNING, "path is requred is root");
    return false;
  }
  config->root.path = root["path"].GetString();
  if (!root.HasMember("readonly")) {
    config->root.readonly = true;
  } else {
    config->root.readonly = root["readonly"].GetBool();
  }
  if (!document.HasMember("mounts")) {
    LOG(WARNING, "mounts is requred");
    return false;
  }
  const rapidjson::Value& mounts = document["mounts"];
  for (rapidjson::SizeType i = 0; i < mounts.Size(); i++) {
      const rapidjson::Value& mount_json = mounts[i];
      Mount mount;
      mount.name = mount_json["name"].GetString();
      mount.path = mount_json["path"].GetString();
      config->mounts.append(mount);
  }
  if (!document.HasMember("platform")) {
    LOG(WARNING, "platform is requred");
    return false;
  }
  config->platform.os = document["platform"]["os"].GetString();
  config->platform.arch = document["platform"]["arch"].GetString();
  if (!document.HasMember("process")) {
    LOG(WARNING, "process is requred");
    return false;
  }
  const rapidjson::Value& process_json = document["process"];
  config->process.terminal = process_json["terminal"].GetBool();
  config->process.cwd = process_json["cwd"].GetString();
  if (!process_json.HasMember("user")) {
    LOG(WARNING, "user is required in process");
    return false;
  }
  const rapidjson::Value& user_json = process_json["user"];
  config->process.user.uid = user_json["uid"].GetInt32();
  config->process.user.gid = user_json["gid"].GetInt32();
  if (!process_json.HasMember("args")) {
    LOG(WARNING, "args is required");
    return false;
  }
  const rapidjson::Value& args_json = process_json["args"];
  for (rapidjson::SizeType i = 0; i < args_json.Size(); i++) {
    config->process.args.append(args_json[i].GetString());
  }

  const rapidjson::Value& envs_json = process_json["env"];
  for (rapidjson::SizeType i = 0; i < envs_json.Size(); i++) {
    config->process.envs.append(envs_json[i].GetString());
  }
  return true;
}

}
}
