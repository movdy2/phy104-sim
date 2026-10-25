# pragma once 
#include <QJsonObject>
#include <QString>

namespace phy104 {

class Jsonloader {
    public:
    static QJsonObject loadObject(const QString& path );
};

}