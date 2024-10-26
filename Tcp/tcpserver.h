#pragma once
#include <map>
#include <netinet/in.h>
#include <list>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "channel.h"
#include "iniparser.h"

#define LISTEN_BACKLOG 511
#define MAX_CONNECTIONS 1024

class TcpServer
{
public:
    static TcpServer* getInstance();
    void run();                                                   // 启动服务器
    void init();                                                  // 初始化
    Channel* getOneToUse();                                       // 获取一个连接池
    void recycle(Channel* channel);                               // 回收连接
    int epollOper(int fd,int event,int opt,int other,Channel*);   // epoll树操作
private:
    bool listenFd();                                              // 启动监听
    void epollInit();                                             // 初始化epoll
    int epollProcessEv(int timer);                                // 处理任务
    bool setNonblocking(int fd);                                  // 设置非阻塞
private:
    static int handleAccept(void* args);                          // 接收连接回调
private:
    TcpServer()=default;
    ~TcpServer()=default;
    TcpServer(const TcpServer&)=delete;
    TcpServer& operator=(const TcpServer&)=delete;
private:
    string m_ip;                                     // ip地址
    uint16_t m_port;                                 // 端口
    int m_lfd=-1;                                    // 监听套接字
    int m_epfd;                                      // epoll套接字
    struct epoll_event m_events[MAX_CONNECTIONS];    // epoll数组
    std::list<Channel*> m_connectionPool;            // 连接池
    Channel* m_mainChannel;                          // 监听套接字单独设置
    std::mutex m_poolMutex;                          // 连接池锁
};