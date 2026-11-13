#pragma once

#include "DataExport.h"
#include "SessionLog.h"

#include <QString>

namespace phy104 {

enum class SeriesExportFormat { Workbook, Csv };

/// Shared file-writing and export-record helpers used by the spreadsheet and PDF pages.
/// Format-specific rendering stays in DataExport / ReportDocument; page widgets own dialogs and choices.
class ExportService final {
public:
    /// A safe base filename with any known export suffix removed.
    static QString fileStem(const QString& requestedName, const QString& fallbackName);
    /// Builds a path with the requested extension after normalizing a user-entered name.
    static QString outputPath(const QString& directory, const QString& requestedName,
                              const QString& fallbackName, const QString& extension);

    /// Atomically writes a CSV or workbook and returns false with a user-readable error on failure.
    static bool writeSeriesFile(const QString& path, const SessionLog& log, const DataSeries& series,
                                SeriesExportFormat format, const ExportOptions& options, QString* error = nullptr);

    /// Creates the common .phy recent-export record after a file has been written successfully.
    static ExportRecord makeRecord(const QString& path, const QString& format, const std::string& series,
                                   int rows, int columns);
};

} // namespace phy104
