#include "threadpool.h"

CThreadPool* CThreadPool::getInstance()
{
    static CThreadPool pool;
    return &pool;
}

bool CThreadPool::create(int threadNum)
{
    m_threadNum=threadNum;
    m_runningNum=0;
    for(int i=0;i<threadNum;i++)
    {
        ThreadItem* item=new ThreadItem(this);
        int err=pthread_create(&item->_Handle,nullptr,threadFunc,item);
        if(err!=0)
        {
            Fatal("thread create failed,errmsg=%s",strerror(errno));
            return false;
        }
        m_pool.push_back(item);
    }
    return true;
}

void CThreadPool::run(int threadNum)
{
    if(!create(threadNum))
        exit(1);
    // 检查是否所有的线程都启动起来
check:
    for(int i=0;i<m_threadNum;i++)
    {
        if(m_pool[i]->_Running==false)
        {
            // 线程还未启动起来,需要等待100ms
            usleep(1000*100);
            goto check;
        }
    }
}

void CThreadPool::inMsgRecvQueueAndSignal(Channel* channel)
{
    m_msgMutex.lock();
    m_msgQueue.push_back(channel->getRecvPackage());
    m_msgMutex.unlock();
    call();
}


void* CThreadPool::threadFunc(void* args)
{
    ThreadItem* item=static_cast<ThreadItem*>(args);
    CThreadPool* pool=item->_pThis;
    // 当线程池暂停
    while (true)
    {
        std::unique_lock<std::mutex> lck(pool->m_msgMutex);
        while(pool->m_msgQueue.empty() && !pool->m_stop)
        {
            // 就绪的线程都会卡在这里
            if(item->_Running==false)
            {
                item->_Running=true;
            }
            // 消息队列为空就必须阻塞
            pool->m_cond.wait(lck);
        }
        // 说明线程要退出了
        if(pool->m_stop)
        {
            lck.unlock();
            break;
        }
        char* buf=pool->m_msgQueue.front();
        pool->m_msgQueue.pop_front();
        lck.unlock();
        ++pool->m_runningNum;
        // 启动业务逻辑
        CLogicHandler::getInstance()->threadRecvProc(buf);
        // 这里处理完后直接销毁
        // TODO
        if(buf)
            delete[] buf;
        --pool->m_runningNum;
    }
    return nullptr;
}

void CThreadPool::call()
{
    m_cond.notify_one();
    if(m_runningNum==m_threadNum)
    {
        time_t currTime=time(NULL);
        if(currTime-m_busyTime>=10)
        {
            m_busyTime=currTime;
            Info("thread is busy,please expanse your threadpool!");
            //TODO: 需要扩容线程数目
        }
    }
}