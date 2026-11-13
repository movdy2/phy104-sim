#pragma once

#include "ExperimentReports.h"

#include <string>
#include <vector>

namespace phy104::ReportSupport {

/// Shared UI-independent construction of the retained-results paused state.
Report pausedReport(const std::string& errorCode, std::vector<FieldIssue> issues,
                    std::vector<std::string> currentInputLines, Notice notice,
                    const ReportContext& context);

/// ASCII case folding for report labels and short English display strings.
std::string lowercaseAscii(std::string text);

} // namespace phy104::ReportSupport
