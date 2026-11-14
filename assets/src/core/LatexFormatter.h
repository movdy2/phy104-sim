#pragma once
#include <string>

namespace phy104::LatexFormatter {

/// 6 significant digits; scientific notation becomes  5.5\times10^{-7}.
std::string number(double value);

/// Unit as LaTeX: "deg" -> ^{\circ},  "m/s" -> \,\text{m/s},  "" -> "".
std::string unit(const std::string& unit);

/// "\theta_{b}=19.4712^{\circ}"
std::string answer(const std::string& symbolLatex, double value, const std::string& unitText);

} // namespace phy104::LatexFormatter
