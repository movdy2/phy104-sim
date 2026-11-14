#include "LatexFormatter.h"

#include <cstdlib>
#include <iomanip>
#include <sstream>

using std::string;
using std::setprecision;
using std::atoi;
using std::to_string;

namespace phy104::LatexFormatter {

string number(double value) {
    std::ostringstream out;
    out << setprecision(6) << value;
    const string text = out.str();

    const auto e = text.find('e');
    if (e == string::npos) return text;

    const string mantissa = text.substr(0, e);
    const int exponent = atoi(text.c_str() + e + 1); // handles "e-07" and "e+20"
    return mantissa + "\\times10^{" + to_string(exponent) + "}";
}

string unit(const string& u) {
    if (u.empty()) return "";
    if (u == "deg") return "^{\\circ}";
    return "\\,\\text{" + u + "}";
}

string answer(const string& symbolLatex, double value, const string& unitText) {
    return symbolLatex + "=" + number(value) + unit(unitText);
}

} // namespace phy104::LatexFormatter