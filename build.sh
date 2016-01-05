#!/usr/bin/env sh
WORK_DIR=`pwd`
THIRD_SRC=$WORK_DIR/thirdsrc
THIRD_PARTY=$WORK_DIR/thirdparty
mkdir -p $THIRD_SRC
mkdir -p $THIRD_PARTY
cd $THIRD_SRC
export PATH=$THIRD_PARTY/bin:$PATH

if [ -f "CMake-3.2.1.tar.gz" ]
then
    echo "CMake-3.2.1.tar.gz exist"
else
    # cmake for gflags
    wget --no-check-certificate -O CMake-3.2.1.tar.gz https://github.com/Kitware/CMake/archive/v3.2.1.tar.gz
    tar zxf CMake-3.2.1.tar.gz
    cd CMake-3.2.1
    ./configure --prefix=$THIRD_PARTY
    make -j4 >/dev/null
    make install
    cd -
fi

if [ -d "$THIRD_SRC/protobuf" ];then
   echo "protobuf exits"
else
   git clone https://github.com/google/protobuf.git
   cd protobuf
   ./autogen.sh
   ./configure --prefix=$THIRD_PARTY
   echo "install protobuf...."
   make -j6 && make install > /dev/null
   echo "install protobuf done"
   cd -
fi

if [ -d "$THIRD_SRC/grpc" ];then
   echo "grpc exits"
else
   git clone https://github.com/grpc/grpc.git
   cd grpc
   git submodule update --init
   echo "install grpc ....."
   make install prefix=$THIRD_PARTY > /dev/null
   echo "install grpc done!"
   cd -
fi
