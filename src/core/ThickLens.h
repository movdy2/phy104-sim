#pragma once
// Thick lens + spherical aberration of a bi-spherical lens. No Qt: plain numbers in, plain numbers out.
//
// Sign convention (the same one the thin-lens screen uses): light travels left -> right, a surface is convex
// towards the incoming light when its radius is positive. A converging (biconvex) lens therefore has R1 > 0
// and R2 < 0; a diverging (biconcave) lens has R1 < 0 and R2 > 0.
//
//   power        P  = (n − 1) (1/R1 − 1/R2) + (n − 1)² t / (n R1 R2)          [1/m or 1/cm, one unit only]
//   focal length f  = 1 / P
//   front vertex V1 -> object-side principal point H1:   h1 = − f (n − 1) t / (n R2)
//   rear  vertex V2 -> image-side principal point H2:    h2 = − f (n − 1) t / (n R1)
//   focal points measured from the vertices:             F1 = h1 − f ,  F2 = h2 + f
//   back focal length BFL = F2, front focal length FFL = −F1
//
// Spherical aberration (third order, stop at the lens, object at infinity):
//   longitudinal SA of a single refracting surface:  Δ = − h² (n₂ − n₁) / (2 n₂ R)
//   summed over both surfaces through the aperture radius h = D/2, converted to the image side by the
//   longitudinal magnification (n_image / n_object) of the remaining refraction.
#include <string>
#include <utility>
#include <vector>

#include "ExperimentReports.h"          // phy104::ThickLensInput

namespace phy104 {

/// `ThickLensInput` (the fields) lives in ExperimentReports.h so every experiment shares one declaration;
/// this namespace only adds the refraction maths.
namespace ThickLens {

/// Classifies an element from the two radii ("Biconvex", "Plano-convex", "Meniscus (converging)" ...).
std::string elementName(double r1, double r2, double power);

/// Everything the screen shows. Never throws; `valid == false` carries a readable `error`.
struct Result {
    bool valid = true;
    std::string error;                              // readable reason when !valid

    double power = 0;                               // 1/cm
    double focalLength = 0;                         // cm, signed
    double h1 = 0, h2 = 0;                          // vertex -> principal point (cm, signed)
    double f1 = 0, f2 = 0;                          // vertex -> focal point (cm, signed)
    double backFocal = 0, frontFocal = 0;           // cm, signed (BFL/F2 and FFL)
    bool   converging = true;
    std::string element;                            // "Biconvex (converging)" ...

    // principal-plane imaging: dₒ measured from H1
    bool   atInfinity = false;
    double objectFromH1 = 0;
    double imageFromH2 = 0;                         // dᵢ (cm)
    double magnification = 0;
    bool   real = false, upright = false;

    // third-order spherical aberration, object at infinity (marginal vs paraxial focus)
    double apertureRadius = 0;                      // h = D/2 (cm)
    double marginalFocus = 0;                       // cm behind V2
    double longitudinalSa = 0;                      // marginalFocus − BFL (cm); < 0 = under-corrected
    double transverseSa = 0;                        // blur radius at the paraxial focus (cm)
    double fNumber = 0;                             // f/# = BFL / D (only meaningful for a converging lens)
    std::string saClass;                            // "UNDER-CORRECTED" / "OVER-CORRECTED" / "NEGLIGIBLE"
};
Result analyse(const ThickLensInput& in);

/// Paraxial image distance for a thin lens of the same focal length, for the comparison line.
double thinLensImage(double objectDistance, double focalLength);

/// Cauchy's two-term dispersion n(λ) = A + B/λ², wavelength in nm (the same law as PhysicsCalculator).
double cauchyIndex(double a, double b, double wavelengthNm);

/// dᵢ vs dₒ of the thick lens (principal planes), dₒ from 1.2|f| to 8|f| - for the chart.
std::vector<std::pair<double, double>> imageCurve(const ThickLensInput& in, int samples = 48);

} // namespace ThickLens
} // namespace phy104
