#!/usr/bin/env sh
WORK_DIR=`pwd`
THIRD_SRC=$WORK_DIR/thirdsrc
THIRD_PARTY=$WORK_DIR/thirdparty
mkdir -p $THIRD_SRC
mkdir -p $THIRD_PARTY

if [ -d "$THIRD_SRC/grpc" ];then
   echo "grpc exits"
else
   git clone https://github.com/grpc/grpc.git
   cd grpc
   git submodule update --init
   echo "install grpc ....."
   make install prefix=$THIRD_PARTY > /dev/null 2>&1
   echo "install grpc done!"
fi
