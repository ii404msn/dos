#include <stdio.h>
#include "version.h"

int main() {
  fprintf(stdout, "version %d.%d \n", KERNEL_VERSION_MAJOR, KERNEL_VERSION_MINOR);
  return 0;
}
