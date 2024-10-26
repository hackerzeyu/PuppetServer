#include "tcpserver.h"

int main()
{
    TcpServer::getInstance()->run();
    return 0;
}