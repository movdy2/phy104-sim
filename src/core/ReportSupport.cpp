#include "ReportSupport.h"

#include <cctype>
#include <utility>

namespace phy104::ReportSupport {
namespace {

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    for (std::size_t pos = 0; (pos = text.find(from, pos)) != std::string::npos; pos += to.size())
        text.replace(pos, from.size(), to);
    return text;
}

} // namespace

std::string lowercaseAscii(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

Report pausedReport(const std::string& errorCode, std::vector<FieldIssue> issues,
                    std::vector<std::string> currentInputLines, Notice notice,
                    const ReportContext& context) {
    Report report;
    report.valid = false;
    report.paused = true;
    report.errorCode = errorCode;
    report.issues = std::move(issues);

    LcdRegister current;
    current.title = "CURRENT INPUT / PAUSED";
    current.resultFirst = true;
    current.resultValue = errorCode;
    current.lines = std::move(currentInputLines);
    current.footer = "NO NEW RESULT / MEMORY RETAINED";
    current.isError = true;
    report.registers.push_back(current);

    const std::string run = lowercaseAscii(context.runLabel);
    const bool haveLast = context.lastValid && context.lastValid->valid && !context.lastValid->retained.title.empty();
    if (haveLast) {
        LcdRegister kept = context.lastValid->retained;
        kept.title = "LAST VALID" + (context.runLabel.empty() ? std::string() : " / " + context.runLabel);
        kept.footer = "STALE / NOT CURRENT INPUTS";
        report.registers.push_back(kept);
        report.headline = context.lastValid->headline;
    }

    notice.error = true;
    notice.body = replaceAll(std::move(notice.body), "%RUN%",
                             context.runLabel.empty() ? "the last valid result" : run);
    report.notices.push_back(std::move(notice));
    report.statusText = errorCode + " · " +
                        (haveLast ? "last valid " + (context.runLabel.empty() ? std::string("result") : run) + " retained"
                                  : "nothing to retain");
    return report;
}

} // namespace phy104::ReportSupport
