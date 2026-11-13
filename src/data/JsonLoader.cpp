#include "JsonLoader.h"

#include <QFile>
#include <QJsonDocument>
#include <stdexcept>

namespace phy104 {

QJsonObject JsonLoader::loadObject(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        throw std::runtime_error("Cannot open data file: " + path.toStdString());

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        throw std::runtime_error("Invalid JSON in " + path.toStdString() + " (offset " +
                                 std::to_string(error.offset) + "): " + error.errorString().toStdString());
    return doc.object();
}

} // namespace phy104
