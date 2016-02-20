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
