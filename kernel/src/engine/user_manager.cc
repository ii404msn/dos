#include "engine/user.h"

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
#include "glog/logging.h"
#include "boost/lexical_cast.hpp"


namespace dos {

UserManager::UserManager() {}
UserManager::~UserManager() {}

// work:x:11651:11651::/home/users/work:/bin/bash
int UserManager::SetUp() {
	user_dict_.clear();
  std::ifstream passwd("/etc/passwd");
  if (!passwd.is_open()) {
    LOG(WARNING, "fail to open /etc/passwd");
    return -1;
  }
  std::vector<int32_t> used_uids;
  std::vector<int32_t> used_gids;
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
    used_uids.push_back(user.uid());
    user.set_gid(boost::lexical_cast<int>(parts[3]));
    used_gids.push_back(user.gid());
    user.home = parts[5];
    LOG(WARNING, "load user %s", parts[0].c_str());
  }
  passwd.close();
  // 10000 accout may be enough
  for (int32_t i =10; i < 10000; i++) {
    if (used_uids.find(i) == used_uids.end()) {
      uids_.push_back(i);
    }
    if (used_gids.find(i) == used_gids.end()) {
      gids_.push_back(i);
    }
  }
  return 0;
}

// work:x:11651:11651::/home/users/work:/bin/bash
int UserManager::AddUser(const User& user) {
  if (user_dict_.find(user.name()) != user_dict_.end()) {
    LOG(WARNING, "user %s does exists", user.name().c_str());
    return 0;
  }
  User& tmp_user = user_dict_[user.name()];
  tmp_user.set_name(user.name());
  gid_t max_gid = 0;
  uid_t max_uid = 0;
  std::map<std::string, User>::iterator it = user_dict_.begin();
  for (; it != user_dict_.end(); ++it) {
    if (it->second.gid > max_gid) {
      max_gid = it->second.gid;
    }
    if (it->second.uid > max_uid) {
      max_uid = it->second.uid;
    }
  }
  tmp_user.gid = uids_.front();
  uids_.pop_front();
  tmp_user.uid = gids_.front();
  gids_.pop_front();
  tmp_user.home = "/home/" + tmp_user.username;
  FILE* passwd_fd = fopen("/etc/passwd", "a");
  fprintf(passwd_fd, "%s:x:%d:%d::%s:/bin/bash\n",
          tmp_user.username.c_str(),
          tmp_user.uid,
          tmp_user.gid,
          tmp_user.home.c_str());
  fclose(passwd_fd);
  FILE* shadow_fd = fopen("/etc/shadow", "a");
  fprintf(shadow_fd, "%s:*LK*:::::::\n", tmp_user.username.c_str());
  fclose(shadow_fd);
  // wangtaize:x:11651:
  FILE* group_fd = fopen("/etc/group", "a");
  fprintf(group_fd, "%s:x:%d:\n", tmp_user.username.c_str(), tmp_user.gid);
  fclose(group_fd);
  return InitBash(tmp_user);
}

int UserManager::GetUser(const std::string& username, User* user) {
  boost::mutex::scoped_lock scoped_lock(mutex_);
  if (user_dict_.find(username) != user_dict_.end()) {
    User& tmp_user = user_dict_[username];
    user->username = tmp_user.username;
    user->gid = tmp_user.gid;
    user->uid = tmp_user.uid;
    user->home = tmp_user.home;
    return 0;
  }
  LOG(WARNING) << "user " << username << "does not exist" ;
  return -1;
}

int UserManager::InitBash(const User& user) {
  std::string base_profile = user.home + "/.bash_profile";
  struct stat path_stat;
  // check home folder
  int ret = stat(user.home.c_str(), &path_stat);
  if (ret != 0 ) {
    ret = mkdir(user.home.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (ret != 0) {
      LOG(FATAL) << "fail to create folder " << user.home << " for " << strerror(errno);
      return ret;
    }
  }
  FILE * profile = fopen(base_profile.c_str(), "w");
  fprintf(profile, "if [ -f /etc/bashrc ]; then\n");
  fprintf(profile, "    . /etc/bashrc\n");
  fprintf(profile, "fi\n");
  fprintf(profile, "PATH=/bin:/usr/bin:/usr/local/bin:/usr/share/bin:.\n");
  fprintf(profile, "export PATH\n");
  fprintf(profile, "HOME=%s\n", user.home.c_str());
  fprintf(profile, "\n");
  fprintf(profile, "\n");
  fclose(profile);
  return 0;
}

}// dos

