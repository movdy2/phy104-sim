#pragma once
// Colour of a spectrum: the CIE 1931 observer (Wyman/Sloan/Shirley analytic fit), integrated against a
// reflectance curve, converted to linear sRGB. No Qt - plain numbers, so the tests can check the physics.
#include <functional>
#include <string>
#include <vector>

namespace phy104 {

struct Rgb {
    double r = 0, g = 0, b = 0;                 // linear sRGB, 0..1 before the display transform
    bool zero() const { return r <= 0 && g <= 0 && b <= 0; }
    std::string hex() const;                    // "#a1b2c3" (gamma-encoded, clipped)
};

namespace Spectrum {

/// The visible range used everywhere (nm).
constexpr double kVisibleMin = 380.0;
constexpr double kVisibleMax = 780.0;

/// The colour of one monochromatic line. Peak-normalised so the brightest hue is (1, 1, 1)-scaled.
Rgb wavelengthToRgb(double wavelengthNm);

/// Integrates a reflectance R(λ) against the CIE observer: the perceived colour of a lit sample.
/// `illuminant` defaults to a flat spectrum (equal energy); `samples` 380..780 nm.
Rgb reflectanceToRgb(const std::function<double(double)>& reflectance, int samples = 161);

/// CIE XYZ tristimulus of one wavelength (the analytic fit, 0 outside the visible range).
void cieXyz(double wavelengthNm, double& x, double& y, double& z);

} // namespace Spectrum
} // namespace phy104
