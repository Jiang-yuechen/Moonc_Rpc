1. 服务端启动前需要启动 Zookeeper 服务器

启动连接(需要虚拟机中的zookeeper在2181端口已经启动)
启动Zookeeper服务器：sudo /usr/share/zookeeper/bin/zkServer.sh start
查看Zookeeper服务器状态：sudo /usr/share/zookeeper/bin/zkServer.sh status
停止Zookeeper服务：sudo /usr/share/zookeeper/bin/zkServer.sh stop
启动Zookeeper客户端并连接到服务器：sudo /usr/share/zookeeper/bin/zkCli.sh -server localhost:2181

2. 主链路
===========================================
服务端启动流程：

读取配置
  ↓
初始化日志
  ↓
创建线程池
  ↓
注册本地 Service 到 ServiceManager
  ↓
启动 RpcServer
  ↓
绑定 IP/端口
  ↓
注册服务地址到 ZooKeeper
  ↓
启动 epoll MessageCycle
  ↓
接收客户端请求
  ↓
协议 Decode
  ↓
ServiceManager 分发到具体业务服务
  ↓
协议 Encode
  ↓
TcpConnection 写回响应

=================================================
客户端启动流程：

读取客户端配置
  ↓
连接 ZooKeeper
  ↓
按 service_name 服务发现
  ↓
负载均衡选择 ip:port
  ↓
序列化请求
  ↓
封装 RpcMessage
  ↓
Socket 连接服务端
  ↓
发送请求
  ↓
接收响应
  ↓
Decode + 反序列化
  ↓
得到 Protobuf/JSON 响应对象