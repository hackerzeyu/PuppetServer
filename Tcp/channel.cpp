#include "channel.h"
#include "tcpserver.h"
#include "threadpool.h"

Channel::Channel(int fd,Handle readCallback,Handle writeCallback,void* args):m_fd(fd),m_currence(0),
    m_readCallback(readCallback),m_writeCallback(writeCallback),m_args(args)
{
}

void Channel::init()
{
    // 清空包头信息
    memset(m_header,0,_HEAD_SIZE_);
    m_precvMemPointer=nullptr;
    m_psendMemPointer=nullptr;
    m_psendBuf=nullptr;
    m_precvBuf=m_header;
    m_recvLen=_HEAD_SIZE_;
    m_curStat=_HEAD_INIT_;
    m_throwSendCount=0;
    m_events=0;
}

int Channel::handleRecv(void* args)
{
    Channel* channel=static_cast<Channel*>(args);
    // 开始接收数据
    ssize_t reco=channel->recvProc();
    if(reco<=0)
    {
        return -1;
    }
    if(channel->m_curStat==_HEAD_INIT_)
    {
        if(reco==channel->m_recvLen)
        {
            // 正好收完包头
            channel->recvBody();
        }
        else
        {
            // 包头没有收完
            channel->m_curStat=_HEAD_RECEIVING_;
            channel->m_precvBuf+=reco;
            channel->m_recvLen-=reco;
        }
    }
    else if(channel->m_curStat==_HEAD_RECEIVING_)
    {
        // 包头收完
        if(channel->m_recvLen==reco)
        {
            channel->recvBody();
        }
        else
        {
            channel->m_precvBuf+=reco;
            channel->m_recvLen-=reco;
        }
    }
    else if(channel->m_curStat==_BODY_INIT_)
    {
        if(reco==channel->m_recvLen)
        {
            // 包体收完,将消息放入线程池
            channel->pushInPool();
        }
        else
        {
            channel->m_curStat=_BODY_RECEIVING_;
            channel->m_precvBuf+=reco;
            channel->m_recvLen-=reco;
        }
    }
    else if(channel->m_curStat==_BODY_RECEIVING_)
    {
        if(channel->m_recvLen==reco)
        {
            channel->pushInPool();
        }
        else
        {
            channel->m_precvBuf+=reco;
            channel->m_recvLen-=reco;
        }
    }
    return 0;
}

int Channel::recvProc()
{
    int n=::recv(m_fd,m_precvBuf,m_recvLen,0);   
    if(n == 0)
    {
        //客户端关闭
        TcpServer::getInstance()->recycle(this);
        return -1;
    }
    //客户端没断，走这里 
    if(n < 0) 
    {
        // 一般不可能,可读有数据才触发呢
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            Error("Unexpected errno:EAGAIN,LT should not appear this!");
            return -1; 
        }
        if(errno == EINTR)  
        {
            Error("Unexpeceted errno:EINTR,interrupted by signal!");
            return -1; 
        }      
        if(errno == ECONNRESET)  
        {       
            Info("client send rst to server!");
        }
        else
        {
            Error("recv error,errmsg=%s",strerror(errno));
        } 
        // 属于有问题的连接,直接回收!
        TcpServer::getInstance()->recycle(this);
        return -1;
    }
    return n;
}

int Channel::recvBody()
{
    lpPkgHeader pkgHeader=(lpPkgHeader)m_header;
    uint16_t msgLen=ntohs(pkgHeader->len);
    // 消息长度比包头小,鉴定为恶意包
    if(msgLen<_HEAD_SIZE_)
    {
        Error("find invalid packet,throw away!!!");
        TcpServer::getInstance()->recycle(this);
        return -1;
    }
    // 消息长度>29000,鉴定为恶意包
    else if(msgLen>=_MAX_MSG_-1000)
    {
        Error("find too long packet,throw away!!!");
        // TODO
        TcpServer::getInstance()->recycle(this);
        return -1;
    }
    else
    {
        // 因为包的长度不固定,所以需要手动分配内存
        char* pTmpBuffer=new char[msgLen+_MSG_HEAD_SIZE_+1];
        memset(pTmpBuffer,0,msgLen+_MSG_HEAD_SIZE_+1);
        m_precvMemPointer=pTmpBuffer;
        // 设置消息头
        MsgHeader msgHeader;
        msgHeader.channel=this;
        msgHeader.currence=m_currence;
        memcpy(pTmpBuffer,&msgHeader,_MSG_HEAD_SIZE_);
        pTmpBuffer+=_MSG_HEAD_SIZE_;
        // 填写包头
        memcpy(pTmpBuffer,pkgHeader,_HEAD_SIZE_);
        pTmpBuffer+=_HEAD_SIZE_;
        int bodyLen=msgLen-_HEAD_SIZE_;
        if(bodyLen==0)
        {
            // 控制包,包体为空
            pushInPool();
        }
        else
        {   
            m_curStat=_BODY_INIT_;
            m_precvBuf=pTmpBuffer;
            m_recvLen=bodyLen;
        }
    }
    return 0;
}

int Channel::handleSend(void* args)
{
    Debug("write event comes...");
    Channel* channel=static_cast<Channel*>(args);
    ssize_t sendSize=channel->sendProc();
    if(sendSize>0 && sendSize!=channel->m_sendLen)
    {
        channel->m_psendBuf+=sendSize;
        channel->m_sendLen-=sendSize;
        return sendSize;
    }
    else if(sendSize==-1)
    {
        // 一般不可能
        Error("write buffer is full,impossible!!!");
        return -1;
    }
    if(sendSize>0 && sendSize==channel->m_sendLen || sendSize==-2)
    {
        TcpServer* server=TcpServer::getInstance();
        if(server->epollOper(channel->m_fd,EPOLLOUT,EPOLL_CTL_MOD,1,channel)==-1)
        {
            Error("handleSend epollOper failed!!!");
            return -1;
        }
    }
    // 消息发送完毕
    if(sendSize>0)
        Debug("send all away successfully!!!");
    delete[] channel->m_psendMemPointer;
    --channel->m_throwSendCount;
    return sendSize;
}

void Channel::pushInPool()
{
    CThreadPool* pool=CThreadPool::getInstance();
    pool->inMsgRecvQueueAndSignal(this);
    init();
}

int Channel::sendProc()
{
    ssize_t n;
    for(;;)
    {
        n=::send(m_fd,m_psendBuf,m_sendLen,0);
        if(n>0)
        {
            return n;
        }
        else if(n==0)
        {
            // 连接断开交给读事件处理
            return 0;
        }
        else if(n<0)
        {
            if(errno==EAGAIN || errno==EWOULDBLOCK)
            {
                // 缓冲区满了
                return -1;
            }
            else if(errno==EINTR)
            {
                // 信号中断
                Error("interrupted by signal!");
                continue;
            }
            else
            {
                break;
            }
        }
    }
    return -2;
}

