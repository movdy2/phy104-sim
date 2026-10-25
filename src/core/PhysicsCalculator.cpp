#include "PhysicsCalculator.h"
#include <cmath>
#include <numbers>
using std::numbers; 
using std::asin; 
using std::optional; 
using std::nullopt;
using std::fabs; 
using std::string; 
//------------------------------------------------
namespace phy104::PhysicsCalculator {

namespace {
    // تحويل بين الراديان والدرجات
    constexpr double kDegToRad = numbers::pi / 180.0; 
    constexpr double kRadToDeg = 180.0 / numbers::pi;
}

RefractionResult refraction(double n1, double n2, double theta1Deg) {
    RefractionResult r; 
    r.reflectedDeg = theta1Deg; 
    
    if (n1 <= 0.0 || n2 <= 0.0) { 
        r.valid = false; 
        return r; 
    }
    
    const double sinTheta2 = n1 * sin(theta1Deg * kDegToRad) / n2; // قانون سنل
    if (fabs(sinTheta2) > 1.0) {
        r.totalInternalReflection = true;
    } else {
        r.refractedDeg = asin(sinTheta2) * kRadToDeg; // ملاحظة: refractedDeg وليست reflectedDeg
    }
    return r; 
}

optional<double> criticalAngleDeg(double n1, double n2) {
    if (n1 <= n2 || n1 <= 0.0 || n2 <= 0.0) return nullopt; 
    return asin(n2 / n1) * kRadToDeg; 
}

optional<double> refractiveIndex(double c, double v) {
    if (v == 0.0) return nullopt;
    return c / v;     
}

ImageResult imageFormation(double s, double f) {
    ImageResult r; 
    if (f == 0.0 || s == 0.0) {
        r.valid = false;
        r.description = (f == 0.0) ? "Invalid focal length (f = 0)" : "Invalid object distance (s = 0)";
        return r;
    }
    
    if (fabs(s - f) < 1e-9) { // الجسم عند البؤرة تماماً
        r.atInfinity = true;
        r.description = "Image at infinity (object at the focal point)";
        return r; 
    }
    
    r.imageDistance = 1.0 / (1.0 / f - 1.0 / s); 
    r.magnification = -r.imageDistance / s; 
    r.real = r.imageDistance > 0.0;
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
    if (lambda * R == 0.0 || d == 0.0) return 0.0;
    const double phase = numbers::pi * d * y / (lambda * R);
    const double c = cos(phase);
    return c * c; // I = I0 cos²(π d y / λR)
}

} // namespace phy104::PhysicsCalculator