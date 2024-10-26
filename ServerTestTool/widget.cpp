#include "widget.h"
#include "ui_widget.h"
#include <QThreadPool>
#include <QMessageBox>
#include "global.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    this->setWindowTitle("压力测试工具");
}

Widget::~Widget()
{
    delete ui;
}


void Widget::on_startBtn_clicked()
{
    if(ui->threadEdit->text().isEmpty())
    {
        QMessageBox::warning(this,"提示","线程数量不能为空");
        return;
    }
    int num=ui->threadEdit->text().toInt();
    if(num<=0 || num>10000)
    {
        QMessageBox::critical(this,"警告","线程数量非法");
        return;
    }
    m_packageNum=num;
    m_threadItems.clear();
    m_finishNum=0;
    for(int i=0;i<num;i++)
    {
        SendThread* sender=new SendThread();
        m_threadItems.push_back(sender);
        connect(sender,&SendThread::finish,this,[=](){
            m_finishNum++;
            m_endTime=time(NULL);
            int time=m_endTime-m_beginTime;
            QString info=QString("发送%1个包,服务器响应所需时间:%2s").arg(m_finishNum).arg(time);
            ui->textEdit->append(info);
            sender->deleteLater();
        });
    }
    // 启动线程开始执行
    m_beginTime=time(NULL);
    for(int i=0;i<num;i++)
    {
        m_threadItems[i]->start();
    }
}

void Widget::on_exitBtn_clicked()
{
    this->close();
}

void Widget::on_clearBtn_clicked()
{
    ui->textEdit->clear();
}


void Widget::on_stopBtn_clicked()
{
    for(int i=0;i<m_packageNum;i++)
    {
        // 强制终止
        auto item=m_threadItems.front();
        m_threadItems.pop_front();
        item->terminate();
        item->wait();
        delete item;
    }
}

