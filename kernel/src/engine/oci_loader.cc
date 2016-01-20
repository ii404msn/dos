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
      const rapidjson::Value& mount = mounts[i]; 
      Mount mount_obj;

  }
}

}
}
