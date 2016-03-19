#include "engine/isolator.h"
#include "gtest/gtest.h"

namespace dos {

class CpuIsolatorTest : public ::testing::Test {

public:
  CpuIsolatorTest(){}
  ~CpuIsolatorTest(){}
};

TEST_F(CpuIsolatorTest, Init) {
  CpuIsolator isolator("/cgroups/cpu/test",
                       "/cgroups/cpuacct/test");
  ASSERT_EQ(1,1);
}

}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
