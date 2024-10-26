#include "global.h"



std::string getSendStr()
{
    QJsonObject obj;
    obj["username"]="admin";
    obj["password"]="123456";
    return QJsonDocument(obj).toJson().toStdString();
}


