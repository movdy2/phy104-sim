#include "ThickLens.h"

#include <algorithm>
#include <cmath>

namespace phy104::ThickLens {
namespace {

} // namespace

std::string elementName(double r1, double r2, double power) {
    const bool flat1 = std::fabs(r1) > 1e5, flat2 = std::fabs(r2) > 1e5;
    const std::string kind = power >= 0 ? "converging" : "diverging";
    if (flat1 && flat2) return "Plano-plano (window)";
    if (flat1) return "Plano-" + std::string(r2 < 0 ? "convex" : "concave") + " (" + kind + ")";
    if (flat2) return "Plano-" + std::string(r1 > 0 ? "convex" : "concave") + " (" + kind + ")";
    if (r1 > 0 && r2 < 0) return std::string(power >= 0 ? "Biconvex" : "Biconvex (negative power)") + " (" + kind + ")";
    if (r1 < 0 && r2 > 0) return std::string(power < 0 ? "Biconcave" : "Biconcave (positive power)") + " (" + kind + ")";
    return "Meniscus (" + kind + ")";
}

Result analyse(const ThickLensInput& in) {
    Result out;
    const auto fail = [&out](const std::string& why) { out.valid = false; out.error = why; return out; };

    if (in.n <= 0) return fail("The lens index n must be positive.");
    if (in.nMedium <= 0) return fail("The surrounding index must be positive.");
    if (in.thickness < 0) return fail("The centre thickness t cannot be negative.");
    if (in.diameter <= 0) return fail("The aperture D must be positive.");
    if (in.objectDistance <= 0) return fail("The object distance dₒ must be positive.");
    if (std::fabs(in.r1) < 1e-9 || std::fabs(in.r2) < 1e-9) return fail("A radius of curvature cannot be zero.");

    const double nn = in.n / in.nMedium;                           // working index (relative)
    const double t = in.thickness;
    const double r1 = in.r1, r2 = in.r2;

    // ---- paraxial power (vergence form) and focal length
    const double p1 = (nn - 1.0) / r1;
    const double p2 = (1.0 - nn) / r2;
    out.power = p1 + p2 - t * p1 * p2 / nn;
    if (std::fabs(out.power) < 1e-12) return fail("This element has no power (P = 0): both surfaces cancel.");
    out.focalLength = 1.0 / out.power;
    out.converging = out.focalLength > 0;
    out.element = elementName(r1, r2, out.power);

    // ---- principal points (measured from the vertices) and focal points
    out.h2 = -out.focalLength * (nn - 1.0) * t / (nn * r1);
    out.h1 = -out.focalLength * (nn - 1.0) * t / (nn * r2);
    out.backFocal = out.h2 + out.focalLength;
    out.frontFocal = out.h1 - out.focalLength;
    out.f2 = out.backFocal;
    out.f1 = out.frontFocal;

    // ---- imaging in the principal planes
    out.objectFromH1 = in.objectDistance + out.h1;                 // dₒ from H1
    if (std::fabs(out.objectFromH1) < 1e-12) {
        out.atInfinity = true;
        out.real = true;
    } else {
        const double inv = 1.0 / out.focalLength - 1.0 / out.objectFromH1;
        if (std::fabs(inv) < 1e-12) {
            out.atInfinity = true;
            out.real = true;
        } else {
            out.imageFromH2 = 1.0 / inv;
            out.magnification = -out.imageFromH2 / out.objectFromH1;
            out.real = out.imageFromH2 > 0;
            out.upright = out.magnification > 0;
        }
    }

    // ---- third-order spherical aberration, object at infinity
    out.apertureRadius = in.diameter / 2.0;
    const double h = out.apertureRadius;
    const double d1 = -h * h * (nn - 1.0) / (2.0 * nn * r1);      // surface 1, light n_medium -> n_lens
    const double d2 = h * h * (nn - 1.0) / (2.0 * r2);            // surface 2, light n_lens -> n_medium
    out.marginalFocus = out.backFocal + d1 + d2;
    out.longitudinalSa = d1 + d2;
    const double na = out.converging ? std::fabs(out.backFocal) / in.diameter : 0.0;
    out.fNumber = na;
    out.transverseSa = na > 0 ? out.longitudinalSa / (2.0 * na) : std::fabs(out.longitudinalSa);
    // Δ < 0: the marginal rays cross the axis in front of the paraxial focus - the usual, under-corrected case
    // of a simple positive lens. A "NEGLIGIBLE" tag is only given when Δ stays inside 1 % of the focal length.
    const double tolerance = 0.01 * std::fabs(out.backFocal) + 1e-9;
    out.saClass = std::fabs(out.longitudinalSa) <= tolerance ? "NEGLIGIBLE"
                : out.longitudinalSa < 0                    ? "UNDER-CORRECTED"
                                                            : "OVER-CORRECTED";
    if (!out.converging) out.saClass = "NOT FOCUSED (diverging)";
    return out;
}

double cauchyIndex(double a, double b, double wavelengthNm) {
    const double um = wavelengthNm / 1000.0;
    return a + b / (um * um);
}

double thinLensImage(double objectDistance, double focalLength) {
    if (std::fabs(focalLength) < 1e-12) return 0.0;
    const double inv = 1.0 / focalLength - 1.0 / objectDistance;
    if (std::fabs(inv) < 1e-12) return 0.0;
    return 1.0 / inv;
}

std::vector<std::pair<double, double>> imageCurve(const ThickLensInput& in, int samples) {
    std::vector<std::pair<double, double>> curve;
    const Result base = analyse(in);
    if (!base.valid) return curve;
    const double f = std::fabs(base.focalLength);
    if (!(f > 0)) return curve;
    const int n = std::max(8, samples);
    for (int k = 0; k <= n; ++k) {
        const double dObj = f * (1.15 + 6.85 * k / n);              // 1.15 f .. 8 f from H1
        const double dV = dObj - base.h1;                          // convert back to the vertex the page measures from
        ThickLensInput sample = in;
        sample.objectDistance = dV > 1e-6 ? dV : 1e-6;
        const Result r = analyse(sample);
        if (!r.valid || r.atInfinity || !std::isfinite(r.imageFromH2)) continue;
        if (std::fabs(r.imageFromH2) > 6 * f) continue;
        curve.emplace_back(dV, r.imageFromH2);
    }
    return curve;
}

} // namespace phy104::ThickLens
