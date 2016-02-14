#include "engine/user_mgr.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <pwd.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "logging.h"
#include "boost/lexical_cast.hpp"

using baidu::common::INFO;
using baidu::common::WARNING;

namespace dos {

UserMgr::UserMgr() {}
UserMgr::~UserMgr() {}

// work:x:11651:11651::/home/users/work:/bin/bash
int UserMgr::SetUp() {
	user_dict_.clear();
  std::ifstream passwd("/etc/passwd");
  if (!passwd.is_open()) {
    LOG(WARNING, "fail to open /etc/passwd");
    return -1;
  }
  std::set<int32_t> used_uids;
  std::set<int32_t> used_gids;
  std::string line;
  while (std::getline(passwd, line)) {
    std::stringstream ss;
    ss << line;
    std::vector<std::string> parts;
    std::string part;
    while (std::getline(ss, part, ':')) {
      parts.push_back(part);
    }
    if (parts.size() < 6) {
      LOG(WARNING, "fail to parse %s to user", line.c_str());
      passwd.close();
      return -1;
    }
    User& user = user_dict_[parts[0]];
    user.set_name(parts[0]);
    user.set_uid(boost::lexical_cast<int>(parts[2]));
    used_uids.insert(user.uid());
    user.set_gid(boost::lexical_cast<int>(parts[3]));
    used_gids.insert(user.gid());
    user.set_home(parts[5]);
    LOG(WARNING, "load user %s", parts[0].c_str());
  }
  passwd.close();
  // 10000 accout may be enough
  for (int32_t i =10; i < 10000; i++) {
    if (used_uids.find(i) == used_uids.end()) {
      uids_.push_back(i);
    }
    // TODO use /etc/group
    if (used_gids.find(i) == used_gids.end()) {
      gids_.push_back(i);
    }
  }
  return 0;
}

// work:x:11651:11651::/home/users/work:/bin/bash
int UserMgr::AddUser(const User& user) {
  if (user_dict_.find(user.name()) != user_dict_.end()) {
    LOG(WARNING, "user %s does exists", user.name().c_str());
    return 0;
  }
  User& tmp_user = user_dict_[user.name()];
  tmp_user.set_name(user.name());
  tmp_user.set_gid(uids_.front());
  uids_.pop_front();
  tmp_user.set_uid(gids_.front());
  gids_.pop_front();
  tmp_user.set_home("/home/" + tmp_user.name());
  FILE* passwd_fd = fopen("/etc/passwd", "a");
  fprintf(passwd_fd, "%s:x:%d:%d::%s:/bin/bash\n",
          tmp_user.name().c_str(),
          tmp_user.uid(),
          tmp_user.gid(),
          tmp_user.home().c_str());
  fclose(passwd_fd);
  FILE* shadow_fd = fopen("/etc/shadow", "a");
  fprintf(shadow_fd, "%s:*LK*:::::::\n", tmp_user.name().c_str());
  fclose(shadow_fd);
  // work:x:11651:
  FILE* group_fd = fopen("/etc/group", "a");
  fprintf(group_fd, "%s:x:%d:\n", tmp_user.name().c_str(), tmp_user.gid());
  fclose(group_fd);
  //return InitBash(tmp_user);
  return 0;
}

int UserMgr::GetUser(const std::string& username, User* user) {
  if (user_dict_.find(username) != user_dict_.end()) {
    User& tmp_user = user_dict_[username];
    user->CopyFrom(tmp_user);
    return 0;
  }
  LOG(WARNING, "user with name %s does not exist", username.c_str());
  return -1;
}

int UserMgr::InitBash(const User& user) {
  std::string base_profile = user.home() + "/.bash_profile";
  struct stat path_stat;
  // check home folder
  int ret = stat(user.home().c_str(), &path_stat);
  if (ret != 0 ) {
    ret = mkdir(user.home().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (ret != 0) {
      LOG(WARNING,"fail to create folder %s for ",strerror(errno));
      return ret;
    }
  }
  FILE * profile = fopen(base_profile.c_str(), "w");
  fprintf(profile, "if [ -f /etc/bashrc ]; then\n");
  fprintf(profile, "    . /etc/bashrc\n");
  fprintf(profile, "fi\n");
  fprintf(profile, "PATH=/bin:/usr/bin:/usr/local/bin:/usr/share/bin:.\n");
  fprintf(profile, "export PATH\n");
  fprintf(profile, "HOME=%s\n", user.home().c_str());
  fprintf(profile, "\n");
  fprintf(profile, "\n");
  fclose(profile);
  return 0;
}

}// dos

