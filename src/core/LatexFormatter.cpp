#include "LatexFormatter.h"

#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace phy104::LatexFormatter {

std::string number(double value) {
    std::ostringstream out;
    out << std::setprecision(6) << value;
    const std::string text = out.str();

    const auto e = text.find('e');
    if (e == std::string::npos) return text;
    const std::string mantissa = text.substr(0, e);
    const int exponent = std::atoi(text.c_str() + e + 1);       // handles "e-07" and "e+20"
    return mantissa + "\\times10^{" + std::to_string(exponent) + "}";
}

std::string unit(const std::string& u) {
    if (u.empty()) return "";
    if (u == "deg") return "^{\\circ}";
    return "\\,\\text{" + u + "}";
}

std::string answer(const std::string& symbolLatex, double value, const std::string& unitText) {
    return symbolLatex + "=" + number(value) + unit(unitText);
}

} // namespace phy104::LatexFormatter
