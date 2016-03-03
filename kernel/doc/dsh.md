### dsh


#### 背景
在 dos 经常会在fork/clone与exec之间执行一些操作，比如 setuid, setgid, sethostname.但是在fork 与exec直接不能执行相关代码
比如LOG, 没法debug这段代码， 所以把这段代码提取出来作为一个进程，它的执行逻辑是，加载配置文件，然后exec用户进程

#### 配置文件格式

配置文件名称为process.yml,存放位置为/dproc/{name}/process.yml
具体内容如下
```
# 进程名称 必须字段
name : 0_container.0_pod.redis
# 主机名称 如果这个字段为空则不设置，非必须字段
hostname : 0_container.0_pod.redis
# 进程运行目录, 在运行进程之前会cd这个目录，必须字段
cwd : /home/redis
# 运行解释器, 必须字段
interceptor : /bin/bash
# args exec args参数
args :
  - bash
  - -c
  - sleep 100
# 环境变量 
envs :
  - CPU=10
  - MEMORY=1G
  - POD=0_pod.redis
# 如果这个字段不为空的话，则将stdout, stdin, stderr定向到 pty设备上面
pty : /dev/pts0
# 是否是initd进程
initd : true
```

#### 无namespace情况下
在dos中有些container是disable namespace的， 在这种情况下，/dproc会放到宿主机的rootfs下面

#### 输入输出存放位置

