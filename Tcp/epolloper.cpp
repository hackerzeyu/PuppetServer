#include "tcpserver.h"

void TcpServer::epollInit()
{
    m_epfd=epoll_create(MAX_CONNECTIONS);
    if(m_epfd==-1)
    {
        Fatal("epoll_create failed!");
        exit(1);
    }
    if(m_lfd==-1)
    {
        Fatal("listen before epoll add lfd!");
        exit(1);
    }
    int ret=epollOper(m_lfd,EPOLLIN|EPOLLRDHUP,EPOLL_CTL_ADD,0,m_mainChannel);
    if(ret==-1)
    {
        exit(1);
    }
}

int TcpServer::epollProcessEv(int timer)
{
//一次最多1024个连接
    int n = ::epoll_wait(m_epfd, m_events, MAX_CONNECTIONS, timer);
    if (n == -1)
    {
        if (errno == EINTR)
        {
            Warn("Unknown signal interrupt!");
            // 信号中断属于正常现象,返回0
            return 0;
        }
        else
        {
            Error("Unknown error in epoll_wait:%s", strerror(errno));
            // 异常
            return -1;
        }
    }
    else if (n == 0)
    {
        // 超时
        if (timer == -1)
        {
            return 0;
        }
        Error("Unexpected return 0 in epoll_wait,error=%s!", strerror(errno));
        return -1;
    }
    for(int i=0;i<n;i++)
    {
        int ev=m_events[i].events;
        int fd=m_events[i].data.fd;
        Channel* channel=(Channel*)m_events[i].data.ptr;
        if(ev & EPOLLIN)
        {
            channel->m_readCallback(channel->getArgs());
        }
        else if(ev & EPOLLOUT)
        {
            Debug("write event comes...");
            channel->m_writeCallback(channel->getArgs());
        }
    }
    return 0;
}

// opt选项如果不是EPOLL_CTL_MOD随便填写
int TcpServer::epollOper(int fd,int event,int opt,int other,Channel* channel)
{
    struct epoll_event ev;
    ev.data.fd=fd;
    if(opt==EPOLL_CTL_ADD)
    {
        ev.events=event;
        channel->setEvent(event);
    }
    else if(opt==EPOLL_CTL_DEL)
    {
        // 暂且不实现,close后会自动下树
        return 0;
    }
    else if(opt==EPOLL_CTL_MOD)
    {
        ev.events=event;
        if(other==0)
        {
            // 添加标记
            ev.events|=event;
        }
        else if(other==1)
        {
            // 去除标记
            ev.events &= ~event;
        }
        else
        {
            // 覆盖标记
            ev.events=event;
        }
        channel->setEvent(ev.events);
    }
    ev.data.ptr=channel;
    int ret=epoll_ctl(m_epfd,opt,fd,&ev);
    if(ret==-1)
    {
        Error("epoll_oper_ctl failed,errmsg=%s",strerror(errno));
    }
    return ret;
}
