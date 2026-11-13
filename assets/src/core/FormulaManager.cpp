#include "NumberFormat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using std::string;
using std::to_string;
using std::abs;

namespace phy104::NumberFormat {

/// تنسيق رقم بعدد محدد من المنازل العشرية (بدون "-0.0000").
string fixed(double value, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value == 0.0 ? 0.0 : value);   // بدون "-0.0000"
    string text = buffer;
    if (text.rfind("-0.", 0) == 0 && text.find_first_not_of("-0.") == string::npos) text.erase(0, 1);
    return text;
}

/// نفس `fixed` لكن يضيف "+" إذا كان الرقم موجباً.
string signedFixed(double value, int decimals) {
    const string text = fixed(value, decimals);
    return (text[0] == '-') ? text : "+" + text;
}

/// تنسيق عام (أقصى دقة ممكنة، بدون أصفار زائدة).
string general(double value, int digits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*g", digits, value == 0.0 ? 0.0 : value);
    return buffer;
}

/// تنسيق علمي باستخدام الرموز العلوية (مثل 5.320 × 10⁻⁷).
string scientific(double value, int decimals) {
    if (value == 0.0) return fixed(0.0, decimals);
    int exponent = static_cast<int>(std::floor(std::log10(std::fabs(value))));
    double mantissa = value / std::pow(10.0, exponent);
    if (std::fabs(std::stod(fixed(mantissa, decimals))) >= 10.0) { mantissa /= 10.0; ++exponent; }
    static const char* superscript[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    string exp = exponent < 0 ? "⁻" : "";
    for (char ch : to_string(abs(exponent))) exp += superscript[ch - '0'];
    return fixed(mantissa, decimals) + " × 10" + exp;
}

} // namespace phy104::NumberFormat
