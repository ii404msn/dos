#include "agent/proc_helper.h"

#include <stdio.h>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include "logging.h"

using ::baidu::common::INFO;
using ::baidu::common::WARNING;
using ::baidu::common::DEBUG;

namespace dos {

bool ProcHelper::LoadMeminfo(Meminfo* meminfo) {
  if (meminfo == NULL) {
    return false;
  }
  FILE* fp = fopen("/proc/meminfo", "rb");
  if (fp == NULL) {
    LOG(WARNING, "fail to open /proc/meminfo");
    return false;
  }
  std::string content;
  char buf[1024];
  int len = 0;
  while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
    content.append(buf, len);
  }
  std::vector<std::string> lines;
  boost::split(lines, content, boost::is_any_of("\n"));
  bool ret = false;
  do {
    for (size_t i = 0; i < lines.size(); i++) {
      std::string line = lines[i];
      std::vector<std::string> parts;
      if (line.find("MemTotal:") == 0) {
        boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() < 2) {
          LOG(WARNING, "fail to parse memtotal %s", line.c_str());
          break;
        }
        meminfo->total= 1024 * boost::lexical_cast<uint64_t>(parts[parts.size() - 2]);
      }else if (line.find("MemFree:") == 0) {
        boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() < 2) {
          LOG(WARNING, "fail to parse memfree %s", line.c_str());
          break;
        }
        meminfo->free = boost::lexical_cast<uint64_t>(parts[parts.size() - 2]);
      }else if (line.find("Buffers:") == 0) {
        boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() < 2) {
          LOG(WARNING, "fail to parse buffer %s", line.c_str());
          break;
        }
        meminfo->buffer = 1024 * boost::lexical_cast<uint64_t>(parts[parts.size() - 2]);
      }else if (line.find("Cached:") == 0) {
        boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
        if (parts.size() < 2) {
          LOG(WARNING, "fail to parse cached %s", line.c_str());
          break;
        }
        meminfo->cached = 1024 * boost::lexical_cast<uint64_t>(parts[parts.size() - 2]);
      }
    }
    ret = true;
  } while(0);
  return ret;
}

bool ProcHelper::LoadCpuinfo(Cpuinfo* cpuinfo) {
  if (cpuinfo == NULL) {
    return false;
  }
  FILE* fp = fopen("/proc/cpuinfo", "rb");
  if (fp == NULL) {
    LOG(WARNING, "fail to open /proc/cpuinfo");
    return false;
  }
  std::string content;
  char buf[4096];
  int len = 0;
  while ((len = fread(buf, 1, sizeof(buf), fp)) > 0) {
    content.append(buf, len);
  }
  std::vector<std::string> lines;
  boost::split(lines, content, boost::is_any_of("\n"));
  cpuinfo->millicores = 0;
  for (size_t index = 0; index < lines.size(); index++) {
    std::string line = lines[index];
    std::vector<std::string> parts;
    if (line.find("processor") == 0) {
      cpuinfo->millicores += 1000;
    }
  }
  return true;
}

} // end of namespace dos

