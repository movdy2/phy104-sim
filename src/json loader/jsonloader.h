# pragma once 
#include <QJsonObject>
#include <QString>

namespace phy104 {

class jsonloader {
    public:
    static QJsonObject loadObject(const QString& path );
};

}