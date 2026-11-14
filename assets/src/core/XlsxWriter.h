#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace phy104 {

struct XlsxCell {
    std::variant<std::monostate, double, std::string> value;
    bool bold = false;

    XlsxCell() = default;
    XlsxCell(double v, bool b = false) : value(v), bold(b) {}
    XlsxCell(const std::string& s, bool b = false) : value(s), bold(b) {}
    XlsxCell(const char* s, bool b = false) : value(std::string(s)), bold(b) {}
};

struct XlsxSheet {
    std::string name;
    std::vector<std::vector<XlsxCell>> rows;
};

/// Builds a valid .xlsx workbook (a ZIP of XML parts, stored without compression) in memory.
/// No external library needed. Strings are written inline, numbers with full precision.
class XlsxWriter {
public:
    static std::vector<std::uint8_t> build(const std::vector<XlsxSheet>& sheets);

    // exposed for tests
    static std::uint32_t crc32(const std::uint8_t* data, std::size_t size);
    static std::string columnName(int zeroBasedIndex);          // 0 -> "A", 27 -> "AB"
    static std::string xmlEscape(const std::string& text);
};

} // namespace phy104
