#pragma once
#include <list>
#include <map>
#include <mutex>
#include <jsoncpp/json/json.h>
#include <pthread.h>
#include <condition_variable>
class Channel;
struct MsgHeader;
#include "channel.h"

// 业务逻辑处理类 
class CLogicHandler
{
public:
    enum LogicCommand
    {
        CMD_EMPTY,
        CMD_LOGIN,
        CMD_REGISTER,
        CMD_COUNT
    };
    struct ThreadItem
    {
        pthread_t _Handle;    // 线程句柄
        bool _Running;        // 线程是否在运行
        CLogicHandler* _pThis;// 线程池对象
        ThreadItem(CLogicHandler* pThis):_pThis(pThis),_Running(false){}
        ~ThreadItem()=default;
    };
    typedef bool (CLogicHandler::*handler)(Channel*,MsgHeader&,char*,uint16_t);
public:
    static CLogicHandler* getInstance();
    void run();            
    void threadRecvProc(char* buf);                                              // 接收数据处理
    bool _HandleRegister(Channel*,MsgHeader&,char* body,uint16_t bodyLength);    // 处理登录
    bool _HandleLogin(Channel*,MsgHeader&,char* body,uint16_t bodyLength);       // 处理登陆
    void msgSend(char* buf);                                                     // 处理数据发送的信号量
    static void* serverSendFunc(void* args);                                     // 线程发送执行函数
    // 初始化发送的channel
    void initSendChannel(Channel* channel,MsgHeader&,const string& str,uint16_t msgCode);             
private:
    CLogicHandler()=default;
    virtual ~CLogicHandler()=default;
    CLogicHandler(const CLogicHandler&)=delete;
    CLogicHandler& operator=(const CLogicHandler&)=delete;
private:
    static const handler m_statusHandle[CMD_COUNT]; // 函数数组
    std::mutex m_logicMutex;                        // 处理业务的锁
    std::condition_variable m_cond;                 // 条件变量
    std::mutex m_sendQueueMutex;                    // 发送消息队列锁
    std::list<char*> m_sendQueue;                   // 发送消息队列
    bool m_stop=false;                              // 终止标记           
};