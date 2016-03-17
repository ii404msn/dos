#include "dsh.h"
#include <stdio.h>

namespace dos {

Dsh::Dsh() {}
Dsh::~Dsh() {}

bool Dsh::Init() {
  fprintf(stdout, "welcome to dos\n");
  fprintf(stdout, "imotai@dos#");
  return true;
}

}// namespace dos
