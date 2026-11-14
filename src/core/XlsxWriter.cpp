#include "XlsxWriter.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace phy104 {
namespace {

struct Part { std::string name; std::string data; };

void put16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}
void put32(std::vector<std::uint8_t>& out, std::uint32_t v) { put16(out, v & 0xFFFF); put16(out, static_cast<std::uint16_t>(v >> 16)); }

std::vector<std::uint8_t> zipStore(const std::vector<Part>& parts) {
    std::vector<std::uint8_t> out;
    struct Entry { std::uint32_t crc, size, offset; };
    std::vector<Entry> entries;
    for (const auto& p : parts) {                               // local file headers + data
        const auto crc = XlsxWriter::crc32(reinterpret_cast<const std::uint8_t*>(p.data.data()), p.data.size());
        entries.push_back({crc, static_cast<std::uint32_t>(p.data.size()), static_cast<std::uint32_t>(out.size())});
        put32(out, 0x04034b50); put16(out, 20); put16(out, 0); put16(out, 0);       // sig, version, flags, method=STORE
        put16(out, 0); put16(out, 0x21);                                            // time, date (1980-01-01)
        put32(out, crc); put32(out, entries.back().size); put32(out, entries.back().size);
        put16(out, static_cast<std::uint16_t>(p.name.size())); put16(out, 0);
        out.insert(out.end(), p.name.begin(), p.name.end());
        out.insert(out.end(), p.data.begin(), p.data.end());
    }
    const std::uint32_t cdStart = static_cast<std::uint32_t>(out.size());
    for (std::size_t i = 0; i < parts.size(); ++i) {            // central directory
        put32(out, 0x02014b50); put16(out, 20); put16(out, 20); put16(out, 0); put16(out, 0);
        put16(out, 0); put16(out, 0x21);
        put32(out, entries[i].crc); put32(out, entries[i].size); put32(out, entries[i].size);
        put16(out, static_cast<std::uint16_t>(parts[i].name.size()));
        put16(out, 0); put16(out, 0); put16(out, 0); put16(out, 0); put32(out, 0);
        put32(out, entries[i].offset);
        out.insert(out.end(), parts[i].name.begin(), parts[i].name.end());
    }
    const std::uint32_t cdSize = static_cast<std::uint32_t>(out.size()) - cdStart;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0);
    put16(out, static_cast<std::uint16_t>(parts.size())); put16(out, static_cast<std::uint16_t>(parts.size()));
    put32(out, cdSize); put32(out, cdStart); put16(out, 0);
    return out;
}

const char* kXmlHead = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
const char* kMain = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";

std::string number(double v) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.15g", v);
    return buffer;
}

std::string sheetXml(const XlsxSheet& sheet) {
    std::string x = std::string(kXmlHead) + "<worksheet xmlns=\"" + kMain + "\"><sheetData>";
    for (std::size_t r = 0; r < sheet.rows.size(); ++r) {
        x += "<row r=\"" + std::to_string(r + 1) + "\">";
        for (std::size_t c = 0; c < sheet.rows[r].size(); ++c) {
            const XlsxCell& cell = sheet.rows[r][c];
            const std::string ref = XlsxWriter::columnName(static_cast<int>(c)) + std::to_string(r + 1);
            const std::string style = cell.bold ? " s=\"1\"" : "";
            if (const auto* text = std::get_if<std::string>(&cell.value))
                x += "<c r=\"" + ref + "\" t=\"inlineStr\"" + style + "><is><t xml:space=\"preserve\">" +
                     XlsxWriter::xmlEscape(*text) + "</t></is></c>";
            else if (const auto* v = std::get_if<double>(&cell.value)) {
                if (std::isfinite(*v)) x += "<c r=\"" + ref + "\"" + style + "><v>" + number(*v) + "</v></c>";
                else x += "<c r=\"" + ref + "\" t=\"inlineStr\"" + style + "><is><t>n/a</t></is></c>";
            }
        }
        x += "</row>";
    }
    return x + "</sheetData></worksheet>";
}

} // namespace

std::uint32_t XlsxWriter::crc32(const std::uint8_t* data, std::size_t size) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

std::string XlsxWriter::columnName(int index) {
    std::string name;
    for (int n = index + 1; n > 0; n = (n - 1) / 26) name.insert(name.begin(), char('A' + (n - 1) % 26));
    return name;
}

std::string XlsxWriter::xmlEscape(const std::string& text) {
    std::string out;
    for (char ch : text) {
        switch (ch) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += ch;
        }
    }
    return out;
}

std::vector<std::uint8_t> XlsxWriter::build(const std::vector<XlsxSheet>& sheets) {
    std::vector<Part> parts;
    const std::string rel = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";

    std::string types = std::string(kXmlHead) + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>";
    for (std::size_t i = 0; i < sheets.size(); ++i)
        types += "<Override PartName=\"/xl/worksheets/sheet" + std::to_string(i + 1) +
                 ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
    types += "</Types>";
    parts.push_back({"[Content_Types].xml", types});

    parts.push_back({"_rels/.rels", std::string(kXmlHead) +
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"" + rel + "/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>"});

    std::string workbook = std::string(kXmlHead) + "<workbook xmlns=\"" + kMain + "\" xmlns:r=\"" + rel + "\"><sheets>";
    std::string wbRels = std::string(kXmlHead) + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    for (std::size_t i = 0; i < sheets.size(); ++i) {
        const std::string id = std::to_string(i + 1);
        workbook += "<sheet name=\"" + xmlEscape(sheets[i].name) + "\" sheetId=\"" + id + "\" r:id=\"rId" + id + "\"/>";
        wbRels += "<Relationship Id=\"rId" + id + "\" Type=\"" + rel + "/worksheet\" Target=\"worksheets/sheet" + id + ".xml\"/>";
        parts.push_back({"xl/worksheets/sheet" + id + ".xml", sheetXml(sheets[i])});
    }
    workbook += "</sheets></workbook>";
    wbRels += "<Relationship Id=\"rId" + std::to_string(sheets.size() + 1) + "\" Type=\"" + rel + "/styles\" Target=\"styles.xml\"/></Relationships>";
    parts.push_back({"xl/workbook.xml", workbook});
    parts.push_back({"xl/_rels/workbook.xml.rels", wbRels});
    parts.push_back({"xl/styles.xml", std::string(kXmlHead) + "<styleSheet xmlns=\"" + kMain + "\">"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font><font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/></cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles></styleSheet>"});
    return zipStore(parts);
}

} // namespace phy104
