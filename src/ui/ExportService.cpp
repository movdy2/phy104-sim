#include "ExportService.h"

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace phy104 {
namespace {

QString normalizedExtension(QString extension) {
    extension = extension.trimmed();
    if (!extension.startsWith('.')) extension.prepend('.');
    return extension;
}

void setError(QString* error, const QString& message) {
    if (error) *error = message;
}

} // namespace

QString ExportService::fileStem(const QString& requestedName, const QString& fallbackName) {
    auto baseName = [](const QString& name) {
        return QFileInfo(QDir::fromNativeSeparators(name.trimmed())).fileName();
    };

    const auto stripKnownSuffix = [](QString name) {
        for (const QString& suffix : {QStringLiteral(".xlsx"), QStringLiteral(".csv"), QStringLiteral(".pdf")}) {
            if (name.endsWith(suffix, Qt::CaseInsensitive)) {
                name.chop(suffix.size());
                break;
            }
        }
        return name;
    };

    QString name = stripKnownSuffix(baseName(requestedName));
    if (name.isEmpty()) name = stripKnownSuffix(baseName(fallbackName));
    return name;
}

QString ExportService::outputPath(const QString& directory, const QString& requestedName,
                                 const QString& fallbackName, const QString& extension) {
    const QString suffix = normalizedExtension(extension);
    return QDir(directory).filePath(fileStem(requestedName, fallbackName) + suffix);
}

bool ExportService::writeSeriesFile(const QString& path, const SessionLog& log, const DataSeries& series,
                                    SeriesExportFormat format, const ExportOptions& options, QString* error) {
    setError(error, QString());

    QByteArray contents;
    if (format == SeriesExportFormat::Workbook) {
        const std::vector<std::uint8_t> workbook = DataExport::buildWorkbook(log, series, options);
        contents = QByteArray(reinterpret_cast<const char*>(workbook.data()), static_cast<qsizetype>(workbook.size()));
    } else {
        contents = QByteArray::fromStdString(DataExport::toCsv(series));
        contents.prepend("\xEF\xBB\xBF", 3); // Excel recognizes UTF-8 Greek symbols and subscripts.
    }

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        setError(error, "Cannot create " + info.absolutePath());
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, "Cannot write " + path + ": " + file.errorString());
        return false;
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        const QString reason = file.errorString();
        file.cancelWriting();
        setError(error, "Could not finish writing " + path + (reason.isEmpty() ? QString() : ": " + reason));
        return false;
    }
    return true;
}

ExportRecord ExportService::makeRecord(const QString& path, const QString& format, const std::string& series,
                                       int rows, int columns) {
    ExportRecord record;
    record.path = path.toStdString();
    record.format = format.toStdString();
    record.series = series;
    record.when = QDateTime::currentDateTime().toString("dd MMM yyyy · HH:mm").toStdString();
    record.rows = rows;
    record.columns = columns;
    return record;
}

} // namespace phy104
