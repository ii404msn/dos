# dos ce package manager design

包管理器要做的几件事情
* 包下载，目前使用wget下载相关包
* 包解压
* 资源隔离
* 并发控制，单位时间最大下载包数

## 包下载

目前支持ftp, http协议下载包

## 包解压

根据文件后缀名，比如tar.gz 使用tar -zxvf 解压，zip使用 unzip解压

## 资源隔离

下载包需要消耗网络io资源，解压需要消耗cpu和disk io ,这两块都会被做资源隔离

## 并发控制

单个节点上面会设置最大并发下载数量，超过最大数，会放入pending队列



