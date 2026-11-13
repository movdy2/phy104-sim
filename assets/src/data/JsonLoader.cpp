#include "JsonLoader.h"

#include <QFile>
#include <QJsonDocument>
#include <stdexcept>

// استخدام using في ملف .cpp للراحة (مسموح)
using std::string;
using std::to_string;
using std::runtime_error;

namespace phy104 {

QJsonObject JsonLoader::loadObject(const QString& path) {
    // تحويل المسار إلى std::string مرة واحدة لاستخدامه في رسائل الخطأ (تحسين أداء)
    const string pathStr = path.toStdString();

    // 1. فتح الملف
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw runtime_error("Cannot open data file: " + pathStr);
    }

    // 2. قراءة وتحليل محتوى الملف
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    
    // 3. التحقق من وجود أخطاء في الصيغة أو إذا لم يكن الكائن الجذري كائناً
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        throw runtime_error("Invalid JSON in " + pathStr + " (offset " +
                            to_string(error.offset) + "): " + error.errorString().toStdString());
    }
    
    // 4. إرجاع الكائن الجذري
    return doc.object();
}

} // namespace phy104