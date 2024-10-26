#include "SendThread.h"
#include "global.h"
#include <iostream>
#include <QDebug>

#pragma pack(1)
typedef struct pkgHeader
{
    uint16_t msgLen;
    uint16_t msgCode;
}*lpkgHeader;
#pragma pack()

SendThread::SendThread()
{
    memset(m_buf,0,sizeof(m_buf));
    pkgHeader header;
    int msgCode=1;
    header.msgCode=::htons(msgCode);
    std::string str=getSendStr();
    m_msgLen=sizeof(header)+str.length();
    header.msgLen=::htons(m_msgLen);
    memcpy(m_buf,&header,sizeof(pkgHeader));
    memcpy(m_buf+sizeof(pkgHeader),str.c_str(),str.length());
    m_buf[strlen(m_buf)]='\0';
}

void SendThread::run()
{
    m_socket=new QTcpSocket();
    QString host=IP;
    m_socket->connectToHost(host,PORT);
    if(m_socket->waitForConnected(10000)){
        m_socket->write(m_buf,m_msgLen);
        if(!m_socket->waitForBytesWritten(10000))
        {
            qDebug()<<"写超时....";
            goto end;
        }
        m_socket->flush();
        if(!m_socket->waitForReadyRead(100000))
        {
            qDebug()<<"接收服务器响应超时...";
            goto end;
        }
        QByteArray response=m_socket->readAll();
        char* data=response.data();
        char* json=data+4;
        QJsonDocument doc=QJsonDocument::fromJson(json);
        if(doc.object()["info"]=="login_success"){
            emit finish();
        }
    }
end:
    m_socket->close();
    delete m_socket;
}
