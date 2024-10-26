#include "tcpserver.h"

int TcpServer::handleAccept(void* args)
{
    auto server=static_cast<TcpServer*>(args);
    int cfd;
    static int use_accept4=1;
    struct sockaddr sockaddr;
    socklen_t socklen=sizeof(sockaddr);
    do
    {
        if(use_accept4)
        {
            cfd=accept4(server->m_lfd,&sockaddr,&socklen,SOCK_NONBLOCK);
        }
        else
        {
            cfd=accept(server->m_lfd,&sockaddr,&socklen);
        }
        if(cfd==-1)
        {
            if(errno==EAGAIN || errno== EWOULDBLOCK)
            {
                // 一般不可能,有链接才触发
                Error("accept failed!");
                return -1;
            }
            else if(errno==ECONNABORTED)
            {
                Error("software abort!");
                return -1;
            }
            else if(errno==EMFILE || errno==ENFILE)
            {
                Error("no fd resource!");
                return -1;
            }
            if(use_accept4 && errno==ENOSYS)
            {
                use_accept4=0;
                continue;
            }
        }
        if(!use_accept4)
        {
            // 设置客户端套接字非阻塞
            if(!server->setNonblocking(cfd))
            {
                Error("set cfd non-blocking failed!");
                close(cfd);
                return -1;
            }
        }
        // 获取一个使用的连接channel
        Channel* channel=server->getOneToUse();
        if(channel==nullptr)
        {
            Error("invalid connection object,channel is null...");
            close(channel->getFd());
            return -1;
        }
        // 设置连接池对象
        channel->setFd(cfd);
        channel->setArgs(channel);
        channel->m_readCallback=Channel::handleRecv;
        channel->m_writeCallback=Channel::handleSend;
        if(server->epollOper(cfd,EPOLLIN|EPOLLRDHUP,EPOLL_CTL_ADD,0,channel)==-1)
        {
            server->recycle(channel);
            return -1;
        }
        Debug("new connection comes,cfd=%d",cfd);
    } while (0);
    return 0;
}