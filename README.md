# 构建状态
[![Travis CI](https://travis-ci.org/imotai/dos.svg?branch=master)](https://travis-ci.org/imotai/dos)
[![Circle CI](https://circleci.com/gh/imotai/dos.svg?style=svg)](https://circleci.com/gh/imotai/dos)

## 构建on centos 

```
git clone https://github.com/imotai/dos.git 
cd dos
yum groupinstall "Development Tools"
yum install cmake
yum install glibc-static.x86_64
yum install zlib-static.x86_64
sh quick_build.sh
```

## 本地运行

```
# 进入sandbox 启动所有模块
cd sandbox && sh start_all.sh
./dos add job -f job.yml
# 获取job信息
./dos get job -n redis
Name:redis
Replica:2
State:kJobNormal
Stat:running 1, death 0, pending 1
CreateTime:2016-03-11 15:25:55
UpdateTime:2016-03-11 15:25:55
# list 本地容器列表
./dos ls container
-  name                     type     state              rtime    btime
---------------------------------------------------------------------------
1  0_container.0_pod.redis  kOci     kContainerPulling  -        -
2  image_fetcher            kSystem  kContainerRunning  10.383m  4.000s
```


