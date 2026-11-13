#pragma once
#include <QJsonObject>
#include <QString>

namespace phy104 {

/// Reads a JSON file and returns its root object.
/// Throws std::runtime_error (readable message) if the file is missing or invalid.
class JsonLoader {
public:
    static QJsonObject loadObject(const QString& path);
};

} // namespace phy104
