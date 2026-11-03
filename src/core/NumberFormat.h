#pragma once
// تنسيق الأرقام المستخدم في سجلات LCD والتصدير (إخراج UTF-8).
#include <string>

namespace phy104::NumberFormat {

/// 4 منازل عشرية افتراضياً: "48.5904"
std::string fixed(double value, int decimals = 4);

/// مع إشارة صريحة: "+15.0000"، "-0.5000"
std::string signedFixed(double value, int decimals = 4);

/// حتى `digits` أرقام معنوية بدون أصفار زائدة: "0.066667"، "1.5"، "30"
std::string general(double value, int digits = 6);

/// صيغة علمية كما في LCD: "5.320 × 10⁻⁷"
std::string scientific(double value, int decimals = 3);

} // namespace phy104::NumberFormat