#pragma once
#include <string>
#include <optional>
using std::string;
using std::optional;
//---------------------------------------------------------------------------------------------------------------------
namespace phy104::PhysicsCalculator {

// ---- A/B: reflection, refraction, total internal reflection ----
struct RefractionResult {
    double reflectedDeg = 0.0;            // law of reflection: θr = θ1
    bool   totalInternalReflection = false;
    double refractedDeg = 0.0;            // valid only if !totalInternalReflection
    bool   valid = true;                  // false for impossible input (n <= 0)
};

RefractionResult refraction(double n1, double n2, double theta1Deg);
optional<double> criticalAngleDeg(double n1, double n2);   // nullopt if n1 <= n2
optional<double> refractiveIndex(double speedOfLight, double speedInMedium);

// ---- C/D: spherical mirror and thin lens (same equations) ----
struct ImageResult {
    bool   valid = true;                  // false when f = 0 or s = 0
    bool   atInfinity = false;
    double imageDistance = 0.0;           // s'
    double magnification = 0.0;           // m = -s'/s
    bool   real = false;
    bool   upright = false;
    string description;                   // "Real, inverted, enlarged"
};

ImageResult imageFormation(double objectDistance, double focalLength);

// ---- E: Young double slit ----
double fringeSpacing(double wavelength, double screenDistance, double slitSeparation);
double fringePosition(int order, double wavelength, double screenDistance, double slitSeparation);

/// Relative intensity (0..1) at screen height y for ideal two-slit interference.
double relativeIntensity(double y, double wavelength, double screenDistance, double slitSeparation);

} // namespace phy104::PhysicsCalculator