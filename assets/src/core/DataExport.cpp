#include "DataExport.h"

#include <cmath>
#include <cstdio>

using std::string;
using std::vector;

namespace phy104::DataExport {
namespace {

string full(double v) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.10g", v);
    return buffer;
}

string csvField(const string& s) {
    if (s.find_first_of(",\"\n") == string::npos) return s;
    string out = "\"";
    for (char c : s) {
        if (c == '"') out += '"';
        out += c;
    }
    return out + "\"";
}

string seriesPrefix(const DataSeries& s) {
    const auto cut = s.id.find('_');
    string base = cut == string::npos ? s.id : s.id.substr(0, cut);
    return base.empty() ? "series" : base;
}

} // namespace

string toCsv(const DataSeries& series) {
    string out;
    for (std::size_t i = 0; i < series.headers.size(); ++i) {
        out += (i ? "," : "") + csvField(series.headers[i]);
    }
    out += "\n";
    for (const auto& row : series.rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            out += (i ? "," : "") + (std::isfinite(row[i]) ? full(row[i]) : string());
        }
        out += "\n";
    }
    return out;
}

vector<XlsxSheet> workbookSheets(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    vector<XlsxSheet> sheets;

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
        vector<XlsxCell> header;
        for (const auto& h : series.headers) header.emplace_back(h, true);
        s.rows.push_back(header);
        for (const auto& row : series.rows) {
            vector<XlsxCell> cells;
            for (double v : row) cells.emplace_back(v);
            s.rows.push_back(cells);
        }
        sheets.push_back(std::move(s));
    }
    if (opt.notes) {
        XlsxSheet s{"Notes", {}};
        s.rows.push_back({XlsxCell("Session notes", true)});
        string line;
        for (char c : log.notes() + "\n") {
            if (c == '\n') { s.rows.push_back({XlsxCell(line)}); line.clear(); }
            else line += c;
        }
        sheets.push_back(std::move(s));
    }
    return sheets;
}

vector<std::uint8_t> buildWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    auto sheets = workbookSheets(log, series, opt);
    if (sheets.empty()) sheets.push_back({"Empty", {{XlsxCell("Nothing selected")}}});
    return XlsxWriter::build(sheets);
}

string describeWorkbook(const SessionLog& log, const DataSeries& series, const ExportOptions& opt) {
    const auto sheets = workbookSheets(log, series, opt);
    const auto bytes = buildWorkbook(log, series, opt).size();
    char size[32];
    std::snprintf(size, sizeof size, "%.1f KB", bytes / 1024.0);
    return std::to_string(sheets.size()) + " sheets · " + std::to_string(series.rows.size()) + " measurements · " + size + " estimated";
}

string suggestedFileName(const DataSeries& series, const string& extension) {
    return "PHY104_" + seriesPrefix(series) + extension;
}

} // namespace phy104::DataExport