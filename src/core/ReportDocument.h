#pragma once
// Printable laboratory report: notes + registers + diagrams + charts in one document.
// Pure C++: the HTML is built here (and unit tested); the Qt layer only attaches the images.
#include "ExperimentReports.h"
#include "SessionLog.h"

#include <string>
#include <utility>
#include <vector>

namespace phy104 {

/// One experiment as it appears in the report.
struct ReportSection {
    std::string title;                                     // "Thin-Film Interference"
    std::string headline;                                  // "I_r/I_r,max = 1.0000"
    std::vector<std::pair<std::string, std::string>> parameters;
    std::vector<LcdRegister> registers;
    std::string diagramKey;                                // "img://diagram-<slug>", empty = no picture
    std::string chartKey;                                  // "img://chart-<slug>", empty = no picture
};

/// What the student ticked on the PDF report screen.
struct ReportOptions {
    std::string title = "PHY104 Laboratory Report";
    std::string subtitle = "Optical Assistant · PHY 104 · Optics";
    std::string generated;                                 // "13 November 2026 · 02:15"
    std::string sessionName = "optics_lab.phy";
    bool notes = true;
    bool registers = true;
    bool diagrams = true;
    bool charts = true;
    bool runs = true;
    int imageWidth = 520;                                  // <img width> in document pixels
};

struct ReportContent {
    std::string notes;                                     // Markdown of the session notes
    std::vector<ReportSection> sections;
    std::vector<RunRecord> runs;
};

/// Print-ready HTML. Pictures are referenced as `img://<key>`; the UI adds them as document resources.
std::string reportHtml(const ReportContent& content, const ReportOptions& options);

/// Minimal Markdown for the notes block: #/##/### headings, `-` bullets, **bold**, `code`, paragraphs.
std::string markdownToHtml(const std::string& markdown);

/// "PHY104_report_2026-11-13" from an ISO date (empty date -> "PHY104_report").
std::string reportFileStem(const std::string& isoDate);

/// "img://diagram-thin-film-interference": the key both the HTML and the Qt resource use.
std::string reportImageKey(const std::string& kind, const std::string& experiment);

} // namespace phy104
