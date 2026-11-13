#pragma once
#include "ExperimentReports.h"
#include "SessionLog.h"
#include "XlsxWriter.h"

#include <string>
#include <vector>

namespace phy104 {

struct ExportOptions {
    bool parameters = true;     // "Input parameters and units"
    bool steps = true;          // "Step-by-step results"
    bool measurements = true;   // "Measurement data & fit"
    bool notes = true;          // "Session notes"
};

namespace DataExport {

/// UTF-8 CSV of one data series (header row + full-precision numbers; NaN -> empty cell).
std::string toCsv(const DataSeries& series);

/// Sheets of the workbook, in this order: Parameters, Steps, Measurements, Notes (only the enabled ones).
std::vector<XlsxSheet> workbookSheets(const SessionLog& log, const DataSeries& series, const ExportOptions& options);

std::vector<std::uint8_t> buildWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& options);

/// "4 sheets · 8 measurements · 12.4 KB estimated"
std::string describeWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& options);

/// Suggested file name: "PHY104_refraction.xlsx"
std::string suggestedFileName(const DataSeries& series, const std::string& extension);

} // namespace DataExport
} // namespace phy104
