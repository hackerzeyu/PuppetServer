#pragma once
#include <atomic>
#include <ctime>
#include <list>
#include <pthread.h>
#include <condition_variable>
#include <vector>
#include "log.h"
#include "channel.h"

class CThreadPool
{
public:
    struct ThreadItem
    {
        pthread_t _Handle;  // 线程句柄
        bool _Running;      // 线程是否在运行
        CThreadPool* _pThis;// 线程池对象
        ThreadItem(CThreadPool* pThis):_pThis(pThis),_Running(false){}
        ~ThreadItem()=default;
    };
public:
    static CThreadPool* getInstance();
    bool create(int threadNum);
    void run(int threadNum);                      // 启动线程池
    // TODO void stop();    
    void inMsgRecvQueueAndSignal(Channel*);       // 入消息队列
private:
    static void* threadFunc(void* args);
    void call();                                  // 唤醒线程
private:
    CThreadPool()=default;
    ~CThreadPool()=default;
    CThreadPool(const CThreadPool&)=delete;
    CThreadPool& operator=(const CThreadPool&)=delete;
private:
    std::vector<ThreadItem*> m_pool;            // 线程池对象
    int m_threadNum;                            // 线程数目
    std::list<char*> m_msgQueue;                // 消息队列
    bool m_stop=false;                          // 是否启动标志
    std::mutex m_msgMutex;                      // 消息队列锁
    std::condition_variable m_cond;             // 条件变量
    std::atomic<int> m_runningNum;              // 正在运行的线程数量
    time_t m_busyTime;                          // 线程繁忙时间
};