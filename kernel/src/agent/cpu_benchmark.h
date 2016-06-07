#ifndef KERNEL_AGENT_CPU_BENCHMARK_H
#define KERNEL_AGENT_CPU_BENCHMARK_H
#include <stdint.h>
namespace dos {

class CpuBenchMark {

public:
  CpuBenchMark();
  ~CpuBenchMark();
  int64_t Start(long count, long accuracy);
private:
  void CalcuPi(long accuracy);
};

}

#endif
