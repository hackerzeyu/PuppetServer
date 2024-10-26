#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QList>
#include <ctime>
#include "sendthread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_startBtn_clicked();

    void on_exitBtn_clicked();

    void on_clearBtn_clicked();

    void on_stopBtn_clicked();

private:
    Ui::Widget *ui;
    QList<SendThread*> m_threadItems;
    int m_packageNum=0;   // 待发送的包数量
    int m_finishNum=0;    // 完成的业务数量
    time_t m_beginTime;   // 开始时间
    time_t m_endTime;     // 结束时间
};
#endif // WIDGET_H
