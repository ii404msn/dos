#include <iostream>
#include <string>


static const std::string GIT_BRANCH=" kill"; 
static const std::string LOG_VERSION="c4dc2f126f4a8e5e7404a9b00d4eb34e806122ee"; 
static const std::string AUTHOR=""; 
static const std::string GIT_URL="https://github.com/imotai/dos.git"; 
static const std::string BUILD_DATE="Sat Mar 19 01:26:08 UTC 2016"; 
static const std::string BUILD_HOSTNAME="vagrant-ubuntu-trusty-64"; 
static const std::string GCC_VERSION="gcc (Ubuntu 4.8.2-19ubuntu1) 4.8.2"; 
static const std::string KERNEL="3.13.0-43-generic"; 

void PrintVersion() {
  std::cout << "Author:" << AUTHOR << std::endl;
  std::cout << "Git:" << GIT_URL << std::endl;
  std::cout << "Branch:" << GIT_BRANCH << std::endl;
  std::cout << "Date:" << BUILD_DATE << std::endl;
  std::cout << "Version:" << LOG_VERSION << std::endl;
  std::cout << "Host:" << BUILD_HOSTNAME << std::endl;
  std::cout << "Gcc:" << GCC_VERSION << std::endl;
  std::cout << "Kernel:" << KERNEL<< std::endl;
}
