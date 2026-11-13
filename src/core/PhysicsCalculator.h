#pragma once
// Pure physics functions used by the virtual experiments (no Qt).
// Angles in degrees, lengths in the SAME unit (e.g. cm or m).
//   s  > 0  real object in front of mirror / lens
//   f  > 0  concave mirror or converging lens, f < 0 convex mirror / diverging lens
//   s' > 0  real image, s' < 0 virtual image
#include <optional>
#include <string>

namespace phy104::PhysicsCalculator {

// ---- A/B: reflection, refraction, total internal reflection ----
struct RefractionResult {
    double reflectedDeg = 0.0;            // law of reflection: θr = θ1
    bool   totalInternalReflection = false;
    double refractedDeg = 0.0;            // valid only if !totalInternalReflection
    bool   valid = true;                  // false for impossible input (n <= 0)
};
RefractionResult refraction(double n1, double n2, double theta1Deg);
std::optional<double> criticalAngleDeg(double n1, double n2);   // nullopt if n1 <= n2
std::optional<double> refractiveIndex(double speedOfLight, double speedInMedium);

// ---- prism (relative index n = n_prism / n_surrounding) ----
struct PrismResult {
    bool   valid = true;           // false for impossible input (n <= 0, apex outside 0..180)
    bool   entryTir = false;       // sin r1 would exceed 1 (cannot happen when going into a denser medium)
    bool   exitTir = false;        // n sin r2 > 1: no emergent ray
    double i1 = 0, r1 = 0, r2 = 0, i2 = 0;   // degrees
    double deviation = 0;          // δ = i1 + i2 - A (degrees), valid only if no TIR
};
PrismResult prism(double nPrism, double nMedium, double apexDeg, double incidenceDeg);
std::optional<double> minimumDeviationDeg(double nPrism, double nMedium, double apexDeg);
/// Incidence angle at which the deviation is minimal (symmetric passage): i = (A + δmin)/2.
std::optional<double> minimumDeviationIncidenceDeg(double nPrism, double nMedium, double apexDeg);
/// n = sin((A + δmin)/2) / sin(A/2)
std::optional<double> indexFromMinimumDeviation(double deltaMinDeg, double apexDeg);
/// Cauchy dispersion n(λ) = A + B/λ², wavelength in nm.
double cauchyIndex(double a, double b, double wavelengthNm);

// ---- C/D: spherical mirror and thin lens (same equations) ----
struct ImageResult {
    bool   valid = true;                  // false when f = 0 or s = 0
    bool   atInfinity = false;
    double imageDistance = 0.0;           // s'
    double magnification = 0.0;           // m = -s'/s
    bool   real = false;
    bool   upright = false;
    std::string description;              // "Real, inverted, enlarged"
};
ImageResult imageFormation(double objectDistance, double focalLength);

// ---- E: Young double slit ----
double fringeSpacing(double wavelength, double screenDistance, double slitSeparation);
double fringePosition(int order, double wavelength, double screenDistance, double slitSeparation);
/// Relative intensity (0..1) at screen height y for ideal two-slit interference.
double relativeIntensity(double y, double wavelength, double screenDistance, double slitSeparation);

} // namespace phy104::PhysicsCalculator
