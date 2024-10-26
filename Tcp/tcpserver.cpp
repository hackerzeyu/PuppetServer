#include "tcpserver.h"
#include "threadpool.h"

TcpServer* TcpServer::getInstance()
{
    static TcpServer server;
    return &server;
}

void TcpServer::init()
{
    INIParser* ini=INIParser::getInstance();
    ini->init("../Conf/server.ini");
    string ip=(*ini)["server"]["ip"];
    int port=(*ini)["server"]["port"];
    int level=(*ini)["log"]["level"];
    int maxLen=(*ini)["log"]["max"];
    bool console=(*ini)["log"]["console"];
    // 初始化日志系统
    auto logger=Logger::getInstance();
    logger->init("../server.log",(Logger::LEVEL)level,maxLen);
    logger->setConsole(console);
    // 初始化服务器
    m_ip=ip;
    m_port=port;
    if(!listenFd())
        exit(1);
    epollInit();
    // 创建一个连接池
    for(int i=0;i<MAX_CONNECTIONS;i++)
    {
        Channel* channel=new Channel();
        m_connectionPool.push_back(channel);
    }
    int recvThreadNum=(*ini)["thread"]["recvNum"];
    // 把线程池启动起来
    auto threadPool=CThreadPool::getInstance();
    threadPool->run(recvThreadNum);
    auto logicHandler=CLogicHandler::getInstance();
    logicHandler->run();
}

bool TcpServer::listenFd()
{
    m_lfd=::socket(AF_INET,SOCK_STREAM,0);
    if(m_lfd<0)
    {
        Fatal("listen socket create failed!");
        return false;
    }

    // 设置地址重用
    int reuseaddr = 1;  //1:打开对应的设置项
    if(setsockopt(m_lfd,SOL_SOCKET, SO_REUSEADDR,(const void *) &reuseaddr, sizeof(reuseaddr)) == -1)
    {
        Fatal("set reuse addr failed!");
        close(m_lfd);                                                
        return false;
    }

    // 设置监听套接字非阻塞
    if(!setNonblocking(m_lfd))
    {
        Fatal("set lfd non-blocking failed!");
        close(m_lfd);
        return false;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family=AF_INET;
    if(m_ip=="")
        serv_addr.sin_addr.s_addr=::htonl(INADDR_ANY);
    else
        serv_addr.sin_addr.s_addr=::inet_addr(m_ip.data());
    serv_addr.sin_port=::htons(m_port);
    
    if(bind(m_lfd,reinterpret_cast<sockaddr*>(&serv_addr),sizeof(serv_addr))<0)
    {
        Fatal("bind address failed!");
        close(m_lfd);
        return false;
    }

    if(listen(m_lfd,LISTEN_BACKLOG)<0)
    {
        Fatal("listen failed!");
        close(m_lfd);
        return false;
    }
    m_mainChannel=new Channel(m_lfd,handleAccept,nullptr,this);
    return true;
}

void TcpServer::run()
{
    this->init();
    Info("server is running...");
    while (true)
    {
        epollProcessEv(-1);
    }
}

bool TcpServer::setNonblocking(int fd)
{
    // 设置套接字非阻塞
    int nb=1;
    return ::ioctl(fd,FIONBIO,&nb)!=-1;
}

Channel* TcpServer::getOneToUse()
{
    std::lock_guard<std::mutex> lck(m_poolMutex);
    Channel* channel;
    if(!m_connectionPool.empty())
    {
        channel=m_connectionPool.front();
        m_connectionPool.pop_front();
    }
    else
    {
        // 没有就创建扩容
        channel=new Channel;
    }
    // 每次使用都必须初始化
    channel->increaseFlag();
    channel->init();
    return channel;
}

void TcpServer::recycle(Channel* channel)
{
    std::lock_guard<std::mutex> lck(m_poolMutex);
    // 关闭套接字和释放内存
    if(channel->getRecvPackage())
        delete[] channel->getRecvPackage();
    if(channel->getSendPackage())
        delete[] channel->getSendPackage();
    channel->increaseFlag();
    close(channel->getFd());
    m_connectionPool.push_back(channel);
    Debug("recycle a channel...");
}


