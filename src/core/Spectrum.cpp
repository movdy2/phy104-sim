#include "Spectrum.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace phy104 {
namespace Spectrum {
namespace {

struct Gamma { double l, a, b; };

// Wyman, Sloan & Shirley (JCGT 2013): multi-lobe fits of the 1931 observer.
double lobe(double x, double mu, double s1, double s2) {
    const double t = (x - mu) / (x < mu ? s1 : s2);
    return std::exp(-0.5 * t * t);
}

double cieX(double w) { return 1.056 * lobe(w, 599.8, 37.9, 31.0) + 0.362 * lobe(w, 442.0, 16.0, 26.7) - 0.065 * lobe(w, 501.1, 20.4, 26.2); }
double cieY(double w) { return 0.821 * lobe(w, 568.8, 46.9, 40.5) + 0.286 * lobe(w, 530.9, 16.3, 31.1); }
double cieZ(double w) { return 1.217 * lobe(w, 437.0, 11.8, 36.0) + 0.681 * lobe(w, 459.0, 26.0, 13.8); }

/// sRGB display transform (the piecewise curve of IEC 61966-2-1).
double encode(double v) {
    v = std::clamp(v, 0.0, 1.0);
    return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}

Rgb xyzToRgb(double X, double Y, double Z) {
    Rgb c;
    c.r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    c.g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    c.b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    c.r = std::max(0.0, c.r); c.g = std::max(0.0, c.g); c.b = std::max(0.0, c.b);
    return c;
}

} // namespace

void cieXyz(double w, double& x, double& y, double& z) {
    if (w < kVisibleMin || w > kVisibleMax) { x = y = z = 0; return; }
    x = cieX(w); y = cieY(w); z = cieZ(w);
}

Rgb wavelengthToRgb(double wavelengthNm) {
    if (wavelengthNm < kVisibleMin || wavelengthNm > kVisibleMax) return {};
    const double x = cieX(wavelengthNm), y = cieY(wavelengthNm), z = cieZ(wavelengthNm);
    // Hue from the tristimulus value, brightness from the photopic response (green is the brightest, the deep
    // red and violet ends are dim). The exponent keeps the ends visible on a dark UI instead of pitch black.
    Rgb c = xyzToRgb(x / y, 1.0, z / y);                     // y normalised to 1: pure hue
    const double peak = std::max({c.r, c.g, c.b});
    if (peak <= 1e-12) return {};
    const double brightness = std::pow(std::clamp(y, 0.0, 1.0), 0.32);
    c.r = c.r / peak * brightness; c.g = c.g / peak * brightness; c.b = c.b / peak * brightness;
    return {encode(c.r), encode(c.g), encode(c.b)};
}

Rgb reflectanceToRgb(const std::function<double(double)>& reflectance, int samples) {
    const int n = std::max(16, samples);
    double sx = 0, sy = 0, sz = 0;
    for (int k = 0; k < n; ++k) {
        const double w = kVisibleMin + (kVisibleMax - kVisibleMin) * k / (n - 1);
        const double x = cieX(w), y = cieY(w), z = cieZ(w);
        const double r = std::clamp(reflectance(w), 0.0, 1.0);
        sx += r * x; sy += r * y; sz += r * z;
    }
    if (sy <= 1e-12) return {};
    // white balance: a perfect mirror must render as white, so divide by the observer's own sums
    double wx = 0, wy = 0, wz = 0;
    for (int k = 0; k < n; ++k) {
        const double w = kVisibleMin + (kVisibleMax - kVisibleMin) * k / (n - 1);
        wx += cieX(w); wy += cieY(w); wz += cieZ(w);
    }
    // ...then adapt that equal-energy white to the sRGB white point (D65 = 0.95047, 1, 1.08883), so a white
    // sample renders white instead of a warm grey.
    Rgb c = xyzToRgb(sx / wx * 0.95047, sy / wy, sz / wz * 1.08883);
    const double peak = std::max({c.r, c.g, c.b});
    if (peak > 1.0) { c.r /= peak; c.g /= peak; c.b /= peak; }
    return {encode(c.r), encode(c.g), encode(c.b)};
}

} // namespace Spectrum

std::string Rgb::hex() const {
    const auto channel = [](double v) { return static_cast<int>(std::lround(std::clamp(v, 0.0, 1.0) * 255.0)); };
    char out[8];
    std::snprintf(out, sizeof out, "#%02x%02x%02x", channel(r), channel(g), channel(b));
    return out;
}

} // namespace phy104
