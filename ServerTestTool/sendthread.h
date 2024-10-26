#ifndef SENDTHREAD_H
#define SENDTHREAD_H
#include <QThread>
#include <QTcpSocket>
#include <winsock2.h>  // Windows Sockets 头文件

struct pkgHeader;

class SendThread:public QThread
{
    Q_OBJECT
public:
    SendThread();
protected:
    void run() override;
signals:
    void finish();
private:
    QTcpSocket *m_socket;
    char m_buf[1024];
    int m_msgLen;
};

#endif // SENDTHREAD_H
