#pragma once
#include <string>
#include <cstdint>

namespace mooncrpc {

/**
 * @brief 底层 Socket 操作工具类
 * @details 封装了 Linux 原生的 POSIX Socket API，提供了极其方便的跨平台调用接口。
 * 这是整个网络通信层的物理基石
 */
class SocketUtils {
public:
    /**
     * @brief 创建一个 TCP Socket 句柄(文件描述符fd)
     * @return 成功返回一个大于 0 的 fd , 失败返回 -1 
     */
    static int CreateTcpSocket();

    /**
     * @brief 绑定 IP 地址和端口号
     * @param fd socket 句柄
     * @param ip 监听的 ip 地址
     * @param port 监听的端口号
     */
    static bool Bind(int fd, const std::string& ip, uint16_t port);

    /**
     * @brief 接收客户端的新连接
     * @param fd socket 句柄
     * @param backlog 全连接队列的最大长度 (默认1024)
     */
    static bool Listen(int fd, int backlog = 1024);

    /**
     * @brief 接收客户端的新连接
     * @param fd 监听 socket 的句柄
     * @param client_ip [输出参数] 连进来的客户端 ip
     * @param client_port [输出参数] 连进来的客户端端口
     * @return 成功返回新建立连接的通信 fd，失败返回 -1
     */
    static int Accept(int fd, std::string& client_ip, uint16_t& client_port);

    /**
     * @brief (客户端专用) 向服务端发起连接
     */
    static bool Connect(int fd, const std::string& server_ip, uint16_t server_port);

    /**
     * @brief 设置 Socket 为非阻塞模式 (Non_blocking)
     * @details 这是 epoll 边缘触发(ET) 模式下必须开启的选项
     */
    static bool SetNonBlocking(int fd);

    /**
     * @brief 设置端口复用 (SO_REUSEADDR)
     * @details 防止服务端异常崩溃重启时，报"Adderss already in use" 的经典 TIME_WAIT 错误 */
    static bool SetReuseAddr(int fd);

    /**
     * @brief 关闭 socket 并释放内核资源
     */
    static void Close(int fd);
};

} //namespace mooncrpc