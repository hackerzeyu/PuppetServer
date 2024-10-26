#include "logic.h"
#include "log.h"
#include "tcpserver.h"
#include <iostream>

// 函数对映表
const CLogicHandler::handler CLogicHandler::m_statusHandle[CLogicHandler::CMD_COUNT] =
    {
        nullptr, // 为空,代表控制信息
        &CLogicHandler::_HandleLogin,
        &CLogicHandler::_HandleRegister
        // TODO
};

void CLogicHandler::run()
{
    pthread_t pid;
    ThreadItem *item = new ThreadItem(this);
    int ret = pthread_create(&pid, nullptr, serverSendFunc, item);
    if (ret < 0)
    {
        Fatal("write thread create failed,errmsg=%s", strerror(errno));
        exit(1);
    }
    while (item->_Running == false)
        ;
}

CLogicHandler *CLogicHandler::getInstance()
{
    static CLogicHandler logicChannel;
    return &logicChannel;
}

void CLogicHandler::threadRecvProc(char *buf)
{
    MsgHeader msgHeader;
    memcpy(&msgHeader, buf, _MSG_HEAD_SIZE_);
    PkgHeader pkgHeader;
    memcpy(&pkgHeader, buf + _MSG_HEAD_SIZE_, _HEAD_SIZE_);
    uint16_t msgLen = ::ntohs(pkgHeader.len);
    uint16_t msgCode = ::ntohs(pkgHeader.msgCode);
    // 记录消息码和连接信息
    Channel *channel = msgHeader.channel;
    Debug("msgCurrence=%d,channelCurrence=%d,msgLen=%d,msgCode=%d", msgHeader.currence, channel->m_currence, msgLen, msgCode);
    // 检测废包
    // (1)如果从收到客户端发送来的包,到服务器释放一个线程池中的线程处理该包的过程中,客户端断开了,所以丢弃
    // (2)该连接对象被其他连接接管了
    // TODO
    if (channel->m_currence != msgHeader.currence)
    {
        // 丢弃
        Error("rubbish package,throw away!!!");
        return;
    }
    if (msgCode >= CMD_COUNT)
    {
        // 恶意包,不能处理
        Error("logic msgCode is invalid,kill!!!");
        TcpServer::getInstance()->recycle(channel);
        return;
    }
    if (m_statusHandle[msgCode] == nullptr)
    {
        // 正常现象,包不需要处理,应该是控制信息
        Info("client send control message!");
        return;
    }
    (this->*m_statusHandle[msgCode])(channel, msgHeader, (char *)(buf + _MSG_HEAD_SIZE_ + _HEAD_SIZE_), msgLen - _HEAD_SIZE_);
}

bool CLogicHandler::_HandleRegister(Channel *channel, MsgHeader &msgHeader, char *body, uint16_t bodyLength)
{
    Json::Reader r;
    Json::Value val;
    if (!r.parse(body, val))
    {
        Error("json string parse error in register!");
        return false;
    }
    return true;
}

bool CLogicHandler::_HandleLogin(Channel *channel, MsgHeader &msgHeader, char *body, uint16_t bodyLength)
{
    // 以下是模拟登录逻辑的测试案例
    if (body == nullptr)
    {
        // 这种属于恶意包,既然不是控制信息,为啥还是空包体
        Error("invalid empty packet...");
        TcpServer::getInstance()->recycle(channel);
        return false;
    }
    Json::Reader r;
    Json::Value val;
    // json解析
    if (!r.parse(body, val))
    {
        Error("json string parse error in login!");
        return false;
    }
    string username = val["username"].asString();
    string password = val["password"].asString();

    Json::Value v;
    // 业务逻辑简单判断
    if (username == "admin" && password == "123456")
    {
        v["info"] = "login_success";
    }
    else
    {
        v["info"] = "login_failed";
    }
    // 序列化json数据
    string json = Json::FastWriter().write(v);
    std::lock_guard<std::mutex> lg(m_sendQueueMutex);
    initSendChannel(channel, msgHeader, json, CMD_LOGIN);
    return true;
}

void CLogicHandler::initSendChannel(Channel *channel, MsgHeader &msgHeader, const string &str, uint16_t msgCode)
{
    uint16_t pkgLen = _HEAD_SIZE_ + str.length();
    char *pTmpBuffer = new char[_MSG_HEAD_SIZE_ + pkgLen + 1];
    memset(pTmpBuffer, 0, _MSG_HEAD_SIZE_ + pkgLen + 1);
    channel->m_psendMemPointer = pTmpBuffer;
    // 设置消息头
    memcpy(pTmpBuffer, &msgHeader, _MSG_HEAD_SIZE_);
    pTmpBuffer += _MSG_HEAD_SIZE_;
    // 设置包头
    PkgHeader pkgHeader;
    pkgHeader.msgCode = ::htons(msgCode);
    pkgHeader.len = ::htons(pkgLen);
    memcpy(pTmpBuffer, &pkgHeader, _HEAD_SIZE_);
    // 设置发送消息
    channel->m_psendBuf = pTmpBuffer;
    pTmpBuffer += _HEAD_SIZE_;
    memcpy(pTmpBuffer, str.data(), str.length());
    // 发送数据前置
    channel->m_sendLen = pkgLen;
    msgSend(channel->m_psendMemPointer);
}

void CLogicHandler::msgSend(char *buf)
{
    m_sendQueue.push_back(buf);
    m_cond.notify_one();
}

void *CLogicHandler::serverSendFunc(void *args)
{
    ThreadItem *item = static_cast<ThreadItem *>(args);
    MsgHeader msgHeader;
    PkgHeader pkgHeader;
    CLogicHandler *handler = item->_pThis;
    std::list<char *>::iterator pos, pos2, end;
    while (!handler->m_stop)
    {
        if (item->_Running == false)
        {
            item->_Running = true;
        }
        std::unique_lock<std::mutex> lck(handler->m_sendQueueMutex);
        while (handler->m_sendQueue.size() <= 0 && !handler->m_stop)
        {
            handler->m_cond.wait(lck);
        }
        pos = handler->m_sendQueue.begin();
        end = handler->m_sendQueue.end();
        Channel *channel;
        // 这段while用于找到一个可以处理的消息
        while (pos != end)
        {
            // 提取消息头和包头
            memcpy(&msgHeader, *pos, _MSG_HEAD_SIZE_);
            memcpy(&pkgHeader, *pos + _MSG_HEAD_SIZE_, _HEAD_SIZE_);
            channel = msgHeader.channel;
            // 包过期,丢弃
            if (channel->m_currence != msgHeader.currence)
            {
                Debug("rubbish package,throw away!!!");
                pos2 = pos;
                pos++;
                handler->m_sendQueue.erase(pos2);
                delete[] channel->m_psendMemPointer;
                continue;
            }
            if (channel->m_throwSendCount > 0)
            {
                // 靠系统驱动来发送消息,这里不能再发送
                pos++;
                continue;
            }
            // 这里才去除
            handler->m_sendQueue.erase(pos);
            break;
        }
        // 说明没有找到可以处理的消息
        if (pos == end)
        {
            continue;
        }
        int sendSize = channel->sendProc();
        if (sendSize > 0)
        {
            if (sendSize == channel->m_sendLen)
            {
                // 数据全部发送
                delete[] channel->m_psendMemPointer;
                channel->m_psendMemPointer = nullptr;
                channel->m_throwSendCount = 0;
            }
            else
            {
                // 标记发送缓冲区满了
                ++channel->m_throwSendCount;
                channel->m_psendBuf += sendSize;
                channel->m_sendLen -= sendSize;
                // 数据没有全部发送
                auto server = TcpServer::getInstance();
                if ((server->epollOper(channel->m_fd, EPOLLOUT, EPOLL_CTL_MOD, 0, channel)) == -1)
                {
                    // 直接让内核发送信息
                    Error("add write event failed!");
                }
            }
            continue;
        }
        else if (sendSize == 0)
        {
            // 连接断开
            delete[] channel->m_psendMemPointer;
            channel->m_psendMemPointer = nullptr;
            channel->m_throwSendCount = 0;
            continue;
        }
        else if (sendSize == -1)
        {
            // 标记发送缓冲区满了
            ++channel->m_throwSendCount;
            // 数据没有全部发送
            auto server = TcpServer::getInstance();
            if ((server->epollOper(channel->m_fd, EPOLLOUT, EPOLL_CTL_MOD, 0, channel)) == -1)
            {
                // 直接让内核发送信息
                Error("add write event failed!");
            }
            continue;
        }
        else
        {
            // 这里一般是对端关闭,或者一些难以想象的错误
            delete[] channel->m_psendMemPointer;
            channel->m_psendMemPointer = nullptr;
            channel->m_throwSendCount = 0;
        }
    }
    return nullptr;
}
