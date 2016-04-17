# 构建状态

[![Join the chat at https://gitter.im/imotai/dos](https://badges.gitter.im/imotai/dos.svg)](https://gitter.im/imotai/dos?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)
[![Travis CI](https://travis-ci.org/imotai/dos.svg?branch=master)](https://travis-ci.org/imotai/dos)
[![Circle CI](https://circleci.com/gh/imotai/dos.svg?style=svg)](https://circleci.com/gh/imotai/dos)
[google forum](https://groups.google.com/forum/#!forum/dos_dev)

## 状态
目前dos尚处于开发阶段
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

## 本地沙盒环境

启动所有模块
```
# 进入sandbox 启动所有模块，会简单部署一个redis实例
cd sandbox && sh quick_test.sh
# 输出结果类似如下
prepare dos.flags done
start nexus
  -  name           type     state              rtime   btime
----------------------------------------------------------------
  1  image_fetcher  kSystem  kContainerRunning  3.000s  4.000s

submit job successfully
  -  name                     type     state              load(us,sys)  rtime    btime
------------------------------------------------------------------------------------------
  1  0_container.0_pod.redis  kOci     kContainerRunning  272,149       38.000s  28.000s
  2  image_fetcher            kSystem  kContainerRunning  0,0           2.067m   4.000s

# 启动的进程类似一下列表
16412 pts/0    Sl     0:01 ./ins --flagfile=ins.flag --server_id=1
16413 pts/0    Sl     0:02 ./ins --flagfile=ins.flag --server_id=2
16414 pts/0    Sl     0:01 ./ins --flagfile=ins.flag --server_id=3
16415 pts/0    Sl     0:00 ./ins --flagfile=ins.flag --server_id=4
16416 pts/0    Sl     0:00 ./ins --flagfile=ins.flag --server_id=5
16918 pts/0    Sl     0:00 ./engine --flagfile=dos.flags
16951 pts/0    Sl     0:00  \_ initd --flagfile=initd.flags
17416 pts/0    Sl     0:00  \_ initd --flagfile=initd.flags
17436 pts/0    Sl     0:00      \_ redis-server *:6379
16967 pts/0    Sl     0:00 ./master --flagfile=dos.flags --master_port=9527
16968 pts/0    Sl     0:00 ./master --flagfile=dos.flags --master_port=9528
16969 pts/0    Sl     0:00 ./master --flagfile=dos.flags --master_port=9529
17148 pts/0    Sl     0:00 ./scheduler --flagfile=dos.flags
17266 pts/0    Sl     0:00 ./doslet --flagfile=dos.flags
```
清空沙盒环境
```
sh stop_all.sh
```
