cmake_minimum_required (VERSION 2.6)
project (dcos_kernel)
set (kernel_version_major 1)
set (kernel_version_minor 0)

configure_file (
    "src/version.h.in"
    "src/version.h"
)
include_directories("./src")
add_executable(master src/master/master_main.cc)
