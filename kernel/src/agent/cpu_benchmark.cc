#include "agent/cpu_benchmark.h"

#include "timer.h"


namespace dos {


CpuBenchMark::CpuBenchMark() {}

CpuBenchMark::~CpuBenchMark() {}

int64_t CpuBenchMark::Start(long count, long accuracy) {
  int64_t now = ::baidu::common::timer::get_microes();
  for (int32_t i = 0; i < count; i++) {
    CalcuPi(accuracy);
  }
  int64_t consume = ::baidu::common::timer::get_microes();
  return consume;
}

void CpuBenchMark::CalcuPi(long accuracy) {
  double pi = 2;
  double last_calc = 0;
  long top = 0;
  long buttom = 0;
  for (long i = 0; i < accuracy; ++i) {
    top += 1;
    buttom += 3;
    if (i == 0) {
      last_calc = top / buttom;
    } else {
      last_calc = last_calc * (top / buttom)
    }
    pi += last_calc;
  }
}

}
