#include "NumberFormat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace phy104::NumberFormat {

std::string fixed(double value, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value == 0.0 ? 0.0 : value);   // no "-0.0000"
    std::string text = buffer;
    if (text.rfind("-0.", 0) == 0 && text.find_first_not_of("-0.") == std::string::npos) text.erase(0, 1);
    return text;
}

std::string signedFixed(double value, int decimals) {
    const std::string text = fixed(value, decimals);
    return (text[0] == '-') ? text : "+" + text;
}

std::string general(double value, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*g", digits, value == 0.0 ? 0.0 : value);
    return buffer;
}

std::string scientific(double value, int decimals) {
    if (value == 0.0) return fixed(0.0, decimals);
    int exponent = static_cast<int>(std::floor(std::log10(std::fabs(value))));
    double mantissa = value / std::pow(10.0, exponent);
    if (std::fabs(std::stod(fixed(mantissa, decimals))) >= 10.0) { mantissa /= 10.0; ++exponent; }   // 9.9996 -> 10.000
    static const char* superscript[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string exp = exponent < 0 ? "⁻" : "";
    for (char ch : std::to_string(std::abs(exponent))) exp += superscript[ch - '0'];
    return fixed(mantissa, decimals) + " × 10" + exp;
}

std::string superscriptInt(int value) {
    static const char* digits[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string out = value < 0 ? "⁻" : "";
    for (char ch : std::to_string(std::abs(value))) out += digits[ch - '0'];
    return out;
}

std::string scientificCompact(double value) {
    if (value == 0.0) return "0";
    int exponent = static_cast<int>(std::floor(std::log10(std::fabs(value))));
    double mantissa = value / std::pow(10.0, exponent);
    if (std::fabs(mantissa) >= 9.99995) { mantissa /= 10.0; ++exponent; }
    if (exponent == 0) return general(mantissa, 6);
    const std::string m = general(mantissa, 6);
    return (m == "1" ? std::string() : m + "×") + "10" + superscriptInt(exponent);
}

} // namespace phy104::NumberFormat
