#include "PhysicsCalculator.h"

#include <cmath>
#include <numbers>

// استخدام using في ملف .cpp للراحة (مسموح)
using std::sin;
using std::cos;
using std::asin;
using std::fabs;
using std::optional;
using std::nullopt;
using std::string;

namespace phy104::PhysicsCalculator {
namespace {
// ثوابت التحويل بين الدرجات والراديان
constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;
}

RefractionResult refraction(double n1, double n2, double theta1Deg) {
    RefractionResult r;
    r.reflectedDeg = theta1Deg;
    if (n1 <= 0.0 || n2 <= 0.0) { r.valid = false; return r; }

    const double sinTheta2 = n1 * sin(theta1Deg * kDegToRad) / n2;   // قانون سنيل
    if (fabs(sinTheta2) > 1.0) {
        r.totalInternalReflection = true;
    } else {
        r.refractedDeg = asin(sinTheta2) * kRadToDeg;    // ملاحظة: refractedDeg وليست reflectedDeg
    }
    return r;
}

optional<double> criticalAngleDeg(double n1, double n2) {
    if (n1 <= n2 || n1 <= 0.0 || n2 <= 0.0) return nullopt;   // الانعكاس الكلي فقط عندما n1 > n2
    return asin(n2 / n1) * kRadToDeg;
}

optional<double> refractiveIndex(double c, double v) {
    if (v == 0.0) return nullopt;
    return c / v;
}

PrismResult prism(double np, double nm, double apexDeg, double i1Deg) {
    PrismResult r;
    r.i1 = i1Deg;
    if (np <= 0.0 || nm <= 0.0 || apexDeg <= 0.0 || apexDeg >= 180.0) { r.valid = false; return r; }

    const double s1 = nm * sin(i1Deg * kDegToRad) / np;
    if (fabs(s1) > 1.0) { r.entryTir = true; return r; }

    r.r1 = asin(s1) * kRadToDeg;
    r.r2 = apexDeg - r.r1;

    const double s2 = np * sin(r.r2 * kDegToRad) / nm;
    if (fabs(s2) > 1.0) { r.exitTir = true; return r; }

    r.i2 = asin(s2) * kRadToDeg;
    r.deviation = r.i1 + r.i2 - apexDeg;
    return r;
}

optional<double> minimumDeviationDeg(double np, double nm, double apexDeg) {
    if (np <= 0.0 || nm <= 0.0 || apexDeg <= 0.0 || apexDeg >= 180.0) return nullopt;
    const double s = (np / nm) * sin(apexDeg * kDegToRad / 2.0);
    if (s > 1.0) return nullopt;
    return 2.0 * asin(s) * kRadToDeg - apexDeg;
}

optional<double> minimumDeviationIncidenceDeg(double np, double nm, double apexDeg) {
    const auto d = minimumDeviationDeg(np, nm, apexDeg);
    if (!d) return nullopt;
    return (apexDeg + *d) / 2.0;
}

optional<double> indexFromMinimumDeviation(double deltaMinDeg, double apexDeg) {
    const double den = sin(apexDeg * kDegToRad / 2.0);
    if (den == 0.0) return nullopt;
    return sin((apexDeg + deltaMinDeg) * kDegToRad / 2.0) / den;
}

double cauchyIndex(double a, double b, double wavelengthNm) {
    const double um = wavelengthNm / 1000.0;
    return a + b / (um * um);
}

ImageResult imageFormation(double s, double f) {
    ImageResult r;
    if (f == 0.0 || s == 0.0) {
        r.valid = false;
        r.description = f == 0.0 ? "Invalid focal length (f = 0)" : "Invalid object distance (s = 0)";
        return r;
    }
    if (fabs(s - f) < 1e-9) {                    // الجسم عند البؤرة تماماً
        r.atInfinity = true;
        r.description = "Image at infinity (object at the focal point)";
        return r;
    }

    r.imageDistance = 1.0 / (1.0 / f - 1.0 / s);      // 1/s + 1/s' = 1/f
    r.magnification = -r.imageDistance / s;           // m = -s'/s
    r.real    = r.imageDistance > 0.0;
    r.upright = r.magnification > 0.0;

    const double absM = fabs(r.magnification);
    const char* size = absM > 1.0001 ? "enlarged" : (absM < 0.9999 ? "reduced" : "same size");
    r.description = string(r.real ? "Real" : "Virtual") + ", " +
                    (r.upright ? "upright" : "inverted") + ", " + size;
    return r;
}

double fringeSpacing(double lambda, double R, double d) {
    if (d == 0.0) return 0.0;
    return lambda * R / d;
}

double fringePosition(int m, double lambda, double R, double d) {
    if (d == 0.0) return 0.0;
    return m * lambda * R / d;
}

double relativeIntensity(double y, double lambda, double R, double d) {
    if (lambda * R == 0.0) return 0.0;
    const double phase = std::numbers::pi * d * y / (lambda * R);
    const double c = cos(phase);
    return c * c;                                      // I = I0 cos²(π d y / λR)
}

} // namespace phy104::PhysicsCalculator
