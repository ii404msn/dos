#ifndef DLFS_DATA_LAYER_H
#define DLFS_DATA_LAYER_H
namespace dos {

struct Buffer {
  const char* data;
  uint32_t length;
  Buffer(const char* buf, uint32_t len)
    :data(buf), length(len) {}
  Buffer()
    :data(NULL), length(0) {}
  Buffer(const Buffer& b)
    :data(b.data), length(b.length) {}
};


class DataLayer {

public:
  DataLayer(const std::string& store_path);
  ~DataLayer();
  bool Write(uint32_t seq, uint64_t offset,
             const char* buf, uint64_t length);
private:
  // flag that mark layer written completely
  bool finished_;
  // cache the request for write
  std::vector<std::pair<const char*, uint64_t> > buf_list_;
};

}
#endif
