#pragma once
#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <functional>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "log.h"
class CLogicHandler;
#include "logic.h"
#pragma pack(1)
typedef struct PkgHeader
{
    uint16_t len;               // 包的总长度
    uint16_t msgCode;           // 包的消息头,用于指令控制
}*lpPkgHeader;
#pragma pack()

typedef struct MsgHeader
{
    int currence;               // 记录废包
    Channel* channel;           // 记录连接对象
}*lpMsgHeader;

#define _HEAD_INIT_       0      // 开始收包头
#define _HEAD_RECEIVING_  1      // 正在收包头
#define _BODY_INIT_       2      // 开始收包体
#define _BODY_RECEIVING_  3      // 正在收包体

#define _HEAD_SIZE_      sizeof(PkgHeader)      // 包头长度
#define _MSG_HEAD_SIZE_  sizeof(MsgHeader)      // 消息头长度

#define _MAX_MSG_        30000   // 包的最大长度

// 回调函数类型
using Handle=std::function<int(void*)>;

class Channel
{
    // 设置友元类CLogicHandler
    friend class CLogicHandler;
public:
    Channel()=default;
    Channel(int fd,Handle readCallback,Handle writeCallback,void* args);
    ~Channel()=default;

    Handle m_readCallback;
    Handle m_writeCallback;

    // 初始化channel
    void init();

    inline void setFd(int fd){m_fd=fd;}
    inline int getFd(){return m_fd;}
    inline void setEvent(int event){m_events=event;}
    inline int getEvent(){return m_events;}
    inline void setArgs(void* args){m_args=args;}
    inline void* getArgs() const{return m_args;}
    // 获取发送或者接收地址
    inline char* getSendPackage() const{return m_psendMemPointer;}
    inline char* getRecvPackage() const{return m_precvMemPointer;}
    // 检测废包
    inline void increaseFlag(){m_currence++;}

    int recvProc();
    int recvBody();
    int sendProc();

    static int handleRecv(void* args);
    static int handleSend(void* args);

    // 将收到消息放入线程池处理
    void pushInPool();
protected:
    int m_fd;                   // 客户端套接字
    int m_events=0;             // 事件标志
    void* m_args;               // 事件回调的参数
    char* m_precvBuf;           // 接收包头位置
    char* m_precvMemPointer;    // 接收包的首地址
    char* m_psendBuf;           // 发送消息位置
    char* m_psendMemPointer;    // 指向发送消息首地址
    int m_recvLen;              // 该接收的长度   
    int m_sendLen;              // 该发送的长度
    char m_header[_HEAD_SIZE_]; // 包头数组       
    unsigned char m_curStat;    // 当前状态
    int m_currence;             // 检测废包
    // 内核发送数量
    std::atomic<int>m_throwSendCount;       
};

