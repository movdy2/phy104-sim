#pragma once
#include <QJsonObject>
#include <QString>

namespace phy104 {

class JsonLoader {
public:
    static QJsonObject loadObject(const QString& path);
};

} // namespace phy104