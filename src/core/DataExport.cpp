#include "DataExport.h"

#include <cmath>
#include <cstdio>

namespace phy104::DataExport {
namespace {

std::string full(double v) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.10g", v);
    return buffer;
}

std::string csvField(const std::string& s) {
    if (s.find_first_of(",\"\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) { if (c == '"') out += '"'; out += c; }
    return out + "\"";
}

std::string seriesPrefix(const DataSeries& s) {
    const auto cut = s.id.find('_');
    std::string base = cut == std::string::npos ? s.id : s.id.substr(0, cut);
    return base.empty() ? "series" : base;
}

} // namespace

std::string toCsv(const DataSeries& series) {
    std::string out;
    for (std::size_t i = 0; i < series.headers.size(); ++i) {
        if (i) out.push_back(',');
        out += csvField(series.headers[i]);
    }
    out.push_back('\n');
    for (const auto& row : series.rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i) out.push_back(',');
            if (std::isfinite(row[i])) out += full(row[i]);
        }
        out.push_back('\n');
    }
    return out;
}

std::vector<XlsxSheet> workbookSheets(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    std::vector<XlsxSheet> sheets;

    if (opt.parameters) {
        XlsxSheet s{"Parameters", {}};
        s.rows.push_back({XlsxCell("Experiment", true), XlsxCell("Parameter", true), XlsxCell("Value", true)});
        for (const char* name : {"Refraction & TIR", "Lenses & Mirrors", "Prisms", "Wave Interference"}) {
            if (const Snapshot* snap = log.snapshot(name))
                for (const auto& [key, value] : snap->parameters)
                    s.rows.push_back({XlsxCell(snap->experiment), XlsxCell(key), XlsxCell(value)});
        }
        sheets.push_back(std::move(s));
    }
    if (opt.steps) {
        XlsxSheet s{"Steps", {}};
        s.rows.push_back({XlsxCell("Experiment", true), XlsxCell("Register", true), XlsxCell("Line", true)});
        for (const char* name : {"Refraction & TIR", "Lenses & Mirrors", "Prisms", "Wave Interference"}) {
            const Snapshot* snap = log.snapshot(name);
            if (!snap) continue;
            for (const auto& reg : snap->registers) {
                for (const auto& line : reg.lines) s.rows.push_back({XlsxCell(snap->experiment), XlsxCell(reg.title), XlsxCell(line)});
                if (!reg.resultValue.empty())
                    s.rows.push_back({XlsxCell(snap->experiment), XlsxCell(reg.title), XlsxCell(reg.resultLabel + " = " + reg.resultValue)});
            }
        }
        sheets.push_back(std::move(s));
    }
    if (opt.measurements) {
        XlsxSheet s{"Measurements", {}};
        std::vector<XlsxCell> header;
        for (const auto& h : series.headers) header.emplace_back(h, true);
        s.rows.push_back(header);
        for (const auto& row : series.rows) {
            std::vector<XlsxCell> cells;
            for (double v : row) cells.emplace_back(v);
            s.rows.push_back(cells);
        }
        sheets.push_back(std::move(s));
    }
    if (opt.notes) {
        XlsxSheet s{"Notes", {}};
        s.rows.push_back({XlsxCell("Session notes", true)});
        std::string line;
        for (char c : log.notes() + "\n") {
            if (c == '\n') { s.rows.push_back({XlsxCell(line)}); line.clear(); } else line += c;
        }
        sheets.push_back(std::move(s));
    }
    return sheets;
}

std::vector<std::uint8_t> buildWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    auto sheets = workbookSheets(log, series, opt);
    if (sheets.empty()) sheets.push_back({"Empty", {{XlsxCell("Nothing selected")}}});
    return XlsxWriter::build(sheets);
}

std::string describeWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    const auto sheets = workbookSheets(log, series, opt);
    const auto bytes = buildWorkbook(log, series, opt).size();
    char size[32];
    std::snprintf(size, sizeof size, "%.1f KB", static_cast<double>(bytes) / 1024.0);
    return std::to_string(sheets.size()) + " sheets · " + std::to_string(series.rows.size()) + " measurements · " + size + " estimated";
}

std::string suggestedFileName(const DataSeries& series, const std::string& extension) {
    return "PHY104_" + seriesPrefix(series) + extension;
}

} // namespace phy104::DataExport
