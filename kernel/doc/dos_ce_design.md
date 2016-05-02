### dos 容器引擎设计

#### feature列表
* 支持opencontainer容器规范
* 支持热升级，托管进程无感知升级
* 支持记录特定条数托管进程状态变化日志
* 支持友好的进程状态信息，比如运行时间

#### 支持opencontainer容器规范
dos容器引擎遵守runtime.json ,config.json配置文件

#### 支持热升级架构

```
    dos_ce(daemon)
          |
          \__rpc__ initd(container1)
                      |
                      \process 1
                      |
                      \process 2
          |
          \__rpc__ initd(container2)
                      |
                      \process 3
                      |
                      \process 4
```
dos_ce 后台进程与initd通过rpc通讯，不要维护进程父子关系,dos_ce后台进程能够随意重启，重启通过本地持久文件，然后找到所有的initd进程
initd 与托管进程维持父子关系，维持父子关系的好处就是，dos容器引擎能够支持每个进程精确的运行状态，
比如退出码，是否coredump等；initd升级流程
* 通过部署替换initd bin
* kill -15 (inidpid)
* initd持久化自己内存状态
* exec 启动命令
* 加载内存状态
这样升级完成依然保持与托管进程父子关系

#### 运行一个容器流程

运行一个容器
* 下载rootfs.tar.gz 并且解压，这两个操作就在一个特殊的子容器里面运行

启动一个全新的initd，并且按照rootfs配置进行mount rootfs



#### 系统任务

image fetcher 用于处理拉包解包 ，并且能够控制并发为了避免拉包解包风暴

#### rootfs文件系统

rootfs文件采用overlayfs


#### 启动initd流程

操作步骤
* 配置主机名，重定向stdout,stdin,stderr,配置账户，等相关操作
* mount rootfs需要的设备，sysfs procfs , chroot
* exec initd 进程

从上面可以看出，在启动initd进程前会执行很多系统调用操作, 但是这些操作放到哪个模块执行了
* 放到clone 与 exec 之间
* clone后直接exec, 然后initd启动时再进行相关操作

第一种方式，实现很简单，因为clone 和 exec之间能够共享内存，直接使用内存相关变量进行相关系统操作，但是存在一些死锁问题，
因为engine是一个多线程环境， clone会复制一块内存，如果内存中某些锁没有释放，然后在子进程去获得锁，则会死锁。
第二章方式，实现略复杂，因为没法使用共享内存，所以需要通过文件把上下文传到子进程里面去，dsh 在单线程环境进行相关操作，然后exec initd

启动initd会使用clone系统调用，但是clone后必须马上进行exec, 如果在clone与exec之间执行一些代码，比如fprintf可能出现死锁情况。
* 生成initd.yml 放到{rootfs}/etc/initd.yml 
* 启动dsh, dsh -c {rootfs}/etc/initd.yml

initd.yml 内容

```
#容器hostname
hostname: 0_container.0_pod.redis
stdout:{rootfs}/log/stdout
stderr:{rootfs}/log/stderr
cwd:/home/redis
uid:0
gid:0
interceptor:/bin/initd
args:
  -initd
  -"--flagfile="
```
#### initd 健康状态check
添加retry_connect_to_initd count 
#### 进程守护
* 定时check进程的退出码
* 执行用户自定义命令或者check一个http url是否返回200
