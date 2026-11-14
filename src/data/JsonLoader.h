#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace phy104 {

/// Reads a JSON file and returns its root object.
/// Throws std::runtime_error (readable message) if the file is missing or invalid.
class JsonLoader {
public:
    static QJsonObject loadObject(const QString& path);
    /// Reads a JSON file whose root is an array (data/study_hub.json): the order of the file is kept.
    static QJsonArray loadArray(const QString& path);
};

} // namespace phy104
