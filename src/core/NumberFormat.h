#pragma once
// Number formatting used by the LCD registers and exports (UTF-8 output).
#include <string>

namespace phy104::NumberFormat {

/// 4 decimal places by default: "48.5904"
std::string fixed(double value, int decimals = 4);
/// With explicit sign: "+15.0000", "-0.5000"
std::string signedFixed(double value, int decimals = 4);
/// Up to `digits` significant digits without trailing zeros: "0.066667", "1.5", "30"
std::string general(double value, int digits = 6);
/// Scientific notation as on the LCD: "5.320 × 10⁻⁷"
std::string scientific(double value, int decimals = 3);

/// "⁻⁹", "³"
std::string superscriptInt(int value);
/// "5×10⁻⁷", "10⁻⁴" (mantissa 1 is dropped), "600" for exponent 0
std::string scientificCompact(double value);

} // namespace phy104::NumberFormat
