#include "WaveExperiments.h"
#include "FieldIds.h"
#include "NumberFormat.h"
#include "ReportSupport.h"
#include "Spectrum.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace phy104::WaveExperiments {
namespace NF = NumberFormat;
namespace {

constexpr double kDeg = std::numbers::pi / 180.0;
std::string fx(double v) { return NF::fixed(v, 4); }
std::string num(double v) { return NF::general(v, 6); }
std::string sub(int i) {
    static const char* d[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
    if (i >= 0 && i < 10) return d[i];
    std::string result{"("};
    result += std::to_string(i);
    result += ')';
    return result;
}
bool isInteger(double v) { return std::fabs(v - std::round(v)) < 1e-9; }

LcdRegister retained(std::vector<std::string> lines, const std::string& label, const std::string& value) {
    LcdRegister r;
    r.title = "LAST VALID";
    r.lines = std::move(lines);
    r.resultLabel = label;
    r.resultValue = value;
    return r;
}

/// Invalid input: nothing is calculated, the last valid result stays on screen (same pattern as the first four screens).
ModelReport paused(const std::string& title, std::vector<FieldIssue> issues, std::vector<std::string> lines,
                   const std::string& guardBody, const ReportContext& ctx) {
    ModelReport report;
    report.report = ReportSupport::pausedReport("E_INPUT_RANGE", std::move(issues), std::move(lines),
                                                {"Calculation paused / " + title, guardBody, true}, ctx);
    return report;
}

Notice interpretation(const std::string& body) { return {"Interpretation", body, false}; }
Notice guard(const std::string& body) { return {"Input guard", body, false}; }

} // namespace

// ================================================================================ Newton's rings
ModelReport newtonRings(const NewtonInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.radiusM <= 0) issues.push_back({FieldIds::radiusOfCurvature, "Enter R > 0 m.", "R"});
    if (in.n <= 0) issues.push_back({FieldIds::refractiveIndex, "Enter n > 0.", "n"});
    if (in.order < 0 || !isInteger(in.order)) issues.push_back({FieldIds::order, "Enter an integer m ≥ 0.", "m"});
    const std::string guardText = "Require λ, R, n > 0 and integer m ≥ 0. This relation assumes normal incidence, contact at the center and r ≪ R.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues,
                      {"λ = " + num(in.lambdaNm) + " nm   R = " + num(in.radiusM) + " m", "n = " + num(in.n) + "   m = " + num(in.order), "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const int m = static_cast<int>(std::lround(in.order));
    const double lam = in.lambdaNm * 1e-9;
    // Transmitted light: both surfaces reverse the phase, the contact point is bright and the dark
    // rings sit at half orders, so the same r² relation is evaluated at m + ½.
    const double order = in.transmitted ? m + 0.5 : double(m);
    const double r2 = order * lam * in.radiusM / in.n;              // r_m² = m'λR/n
    const double r = std::sqrt(r2);
    const double t = order * lam / (2 * in.n);                      // film thickness at ring m
    const std::string ms = sub(m);
    const std::string orderExpr = in.transmitted ? "(" + std::to_string(m) + " + ½)" : std::to_string(m);

    ModelReport out;
    Report& rep = out.report;
    LcdRegister a;
    a.title = in.transmitted ? "TRANSMITTED DARK RING" : "REFLECTED DARK RING";
    a.lines = {std::string("rₘ² = ") + (in.transmitted ? "(m + ½)λR / n" : "mλR / n"),
               " = " + orderExpr + " × " + num(in.lambdaNm) + "×10⁻⁹ × " + num(in.radiusM) + " / " + num(in.n),
               " = " + NF::scientific(r2, 4) + " m²", "r" + ms + " = √(" + NF::scientificCompact(r2).substr(0, 0) + NF::general(r2 * 1e6, 5) + "×10⁻⁶) m"};
    a.resultLabel = "RADIUS · r" + ms;
    a.resultValue = fx(r * 1e3) + " mm";
    LcdRegister b;
    b.title = "DIAMETER / FILM THICKNESS";
    b.lines = {"D" + ms + " = 2r" + ms,
               "t" + ms + " = " + (in.transmitted ? std::string("(m+½)λ/(2n)") : std::string("mλ/(2n)")) + " = " + NF::general(t * 1e9, 6) + " nm",
               in.transmitted ? "m = 0 → bright center, first dark at ½ order" : "m = 0 → r₀ = 0 (dark)"};
    b.resultLabel = "DIAMETER · D" + ms;
    b.resultValue = fx(2 * r * 1e3) + " mm";
    rep.registers = {a, b};
    rep.notices = {interpretation(in.transmitted
                       ? "On transmission both surfaces reverse the phase, so the contact point is bright and the dark rings sit at half orders: squared radii still grow linearly, but with an offset of ½λR/n."
                       : "One reflection phase reversal makes the contact point dark. Squared radius, not radius, grows linearly with dark-ring order."),
                   guard(guardText)};
    rep.headline = "r" + ms + " = " + fx(r * 1e3) + " mm";
    rep.retained = retained({"λ = " + NF::fixed(in.lambdaNm, 1) + " nm · R = " + NF::fixed(in.radiusM, 4) + " m", "n = " + NF::fixed(in.n, 4) + " · m = " + std::to_string(m)}, "RETAINED RADIUS · r" + ms, a.resultValue);
    rep.parameters = {{"λ", num(in.lambdaNm) + " nm"}, {"R", num(in.radiusM) + " m"}, {"m", std::to_string(m)}, {"n", num(in.n)},
                      {"mode", in.transmitted ? "transmitted" : "reflected"}};

    const double slope = lam * in.radiusM / in.n * 1e6;             // mm² per order
    ChartData& c = out.chart;
    c.xLabel = "m (order)"; c.yLabel = "rₘ² (mm²)";
    c.xMin = 0; c.xMax = std::max(6.0, std::ceil(in.order * 1.0));
    c.yMin = 0; c.yMax = slope * c.xMax;
    c.xTicks = static_cast<int>(c.xMax); c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
    c.curve = {{0, 0}, {c.xMax, slope * c.xMax}};
    c.marker = {{double(m), slope * order}};                            // transmitted: half-order ring
    c.legend = "Slope = λR/n = " + fx(slope) + " mm²/order   ● m = " + std::to_string(m);
    return out;
}

// ================================================================================ Michelson interferometer
ModelReport michelson(const MichelsonInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.n < 1) issues.push_back({FieldIds::refractiveIndex, "Enter n ≥ 1.", "n"});
    const std::string guardText = "Require λ > 0 and n ≥ 1. Equal-amplitude beams give full visibility; unequal beams reduce fringe contrast.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues, {"λ = " + num(in.lambdaNm) + " nm", "n = " + num(in.n), "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const double opd = 2 * in.n * in.displacementUm * 1e3;          // nm
    const double shift = opd / in.lambdaNm;                         // fringes
    const double phase = 2 * std::numbers::pi * shift;
    const double v = in.visibility;
    const double intensity = (1 + v * std::cos(phase)) / (1 + v);

    ModelReport out;
    Report& rep = out.report;
    if (in.fringes) {
        // Second tab: hold the mirror and read the pattern itself — spacing, count and contrast.
        const double spacingNm = in.lambdaNm / (2 * in.n);          // mirror travel per fringe
        const double sweepUm = 2.0;                                 // the chart spans ±2 µm
        const double count = 2 * in.n * (2 * sweepUm * 1e3) / in.lambdaNm;
        LcdRegister a;
        a.title = "FRINGE SPACING";
        a.lines = {"Δx = λ / (2n) per fringe", " = " + num(in.lambdaNm) + " nm / (2 × " + fx(in.n) + ")",
                   " = " + fx(spacingNm) + " nm of mirror travel", "2n/λ = " + fx(2 * in.n / (in.lambdaNm * 1e-3)) + " fringes per µm"};
        a.resultLabel = "MIRROR MOTION PER FRINGE";
        a.resultValue = fx(spacingNm) + " nm";
        LcdRegister b;
        b.title = "COUNT ACROSS A ±2 µm SWEEP";
        b.lines = {"N = 2nΔx / λ", " = 2 × " + fx(in.n) + " × " + NF::fixed(2 * sweepUm * 1e3, 1) + " nm / " + num(in.lambdaNm) + " nm",
                   "V = " + NF::fixed(v, 2) + (v >= 1 ? "  (equal beams)" : "  (unequal beams)"),
                   "I/Imax = (1 + V cos 2πN)/(1 + V)"};
        b.resultLabel = "FRINGES IN ±2 µm";
        b.resultValue = fx(count) + " fringes";
        rep.registers = {a, b};
        rep.notices = {interpretation("Every λ/(2n) = " + fx(spacingNm) + " nm of mirror travel adds one round-trip wavelength, so one more fringe crosses the detector. Unequal beams change only the contrast V = " + NF::fixed(v, 2) + ", never the spacing."),
                       guard(guardText)};
        rep.headline = fx(count) + " fringes in ±2 µm";
        rep.retained = retained({"λ = " + NF::fixed(in.lambdaNm, 1) + " nm · n = " + fx(in.n),
                                 "sweep Δx = ±" + NF::fixed(sweepUm, 1) + " µm · V = " + NF::fixed(v, 2)},
                                "RETAINED FRINGE SPACING", a.resultValue);
        rep.parameters = {{"λ", num(in.lambdaNm) + " nm"}, {"Δx", num(in.displacementUm) + " µm"}, {"n", num(in.n)},
                          {"V", num(v)}, {"mount", in.piezo ? "piezo scan" : "micrometer"}, {"mode", "fringes"}};

        ChartData& c = out.chart;
        c.xLabel = "Δx (µm)"; c.yLabel = "I / Imax";
        c.xMin = -sweepUm; c.xMax = sweepUm; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 1; c.yDecimals = 1;
        for (int k = 0; k <= 600; ++k) {
            const double x = -sweepUm + 2 * sweepUm * k / 600.0;
            c.curve.push_back({x, (1 + v * std::cos(2 * std::numbers::pi * (2 * in.n * x * 1e3) / in.lambdaNm)) / (1 + v)});
        }
        const double marked = std::clamp(in.displacementUm, -sweepUm, sweepUm);
        c.marker = {{marked, (1 + v * std::cos(2 * std::numbers::pi * (2 * in.n * marked * 1e3) / in.lambdaNm)) / (1 + v)}};
        c.legend = "λ/(2n) = " + fx(spacingNm) + " nm per fringe   ● current Δx";
        return out;
    }
    LcdRegister a;
    a.title = "ROUND-TRIP PATH CHANGE";
    a.lines = {"ΔOPD = 2nΔx", " = 2 × " + fx(in.n) + " × " + NF::signedFixed(in.displacementUm) + " µm", " = " + NF::signedFixed(opd / 1e3) + " µm",
               "ΔN = ΔOPD / λ", " = " + NF::general(opd, 6) + " nm / " + num(in.lambdaNm) + " nm"};
    a.resultLabel = "FRINGE SHIFT · ΔN";
    a.resultValue = NF::signedFixed(shift);
    LcdRegister b;
    b.title = "PHASE-REFERENCED DETECTOR";
    b.lines = {"Δφ = 2πΔN = " + NF::signedFixed(phase) + " rad", "I/Imax = (1 + V cos Δφ)/(1 + V)", "V = " + NF::fixed(v, 2) + (v >= 1 ? "  (equal beams)" : "  (unequal beams)")};
    b.resultLabel = "NORMALIZED INTENSITY · I/Imax";
    b.resultValue = fx(intensity);
    rep.registers = {a, b};
    rep.notices = {interpretation("Moving one mirror by λ/(2n) = " + NF::fixed(in.lambdaNm / (2 * in.n) / 1e3, 4) + " µm adds one full round-trip wavelength and shifts the pattern by one fringe."), guard(guardText)};
    rep.headline = "ΔN = " + NF::signedFixed(shift) + " fringes";
    rep.retained = retained({"λ = " + NF::fixed(in.lambdaNm, 1) + " nm · n = " + fx(in.n), "Δx = " + NF::signedFixed(in.displacementUm) + " µm"}, "RETAINED FRINGE SHIFT · ΔN", a.resultValue);
    rep.parameters = {{"λ", num(in.lambdaNm) + " nm"}, {"Δx", num(in.displacementUm) + " µm"}, {"n", num(in.n)}, {"V", num(v)},
                      {"mount", in.piezo ? "piezo scan" : "micrometer"}, {"mode", "displacement"}};

    ChartData& c = out.chart;
    c.xLabel = "Δx (µm)"; c.yLabel = "I / Imax";
    c.xMin = -1; c.xMax = 1; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 1; c.yDecimals = 1;
    for (int k = 0; k <= 400; ++k) {
        const double x = -1 + k * 0.005;
        c.curve.push_back({x, (1 + v * std::cos(2 * std::numbers::pi * (2 * in.n * x * 1e3) / in.lambdaNm)) / (1 + v)});
    }
    c.marker = {{in.displacementUm, intensity}};
    c.legend = "I/Imax = (1 + V cos 4πnΔx/λ)/(1 + V)   ● current Δx";
    return out;
}

// ================================================================================ thin film
ModelReport thinFilm(const ThinFilmInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.n <= 0) issues.push_back({FieldIds::refractiveIndex, "Enter n > 0.", "n"});
    if (in.thicknessNm <= 0) issues.push_back({FieldIds::thickness, "Enter t > 0 nm.", "t"});
    const std::string guardText = "Require λ, n, t > 0. Assumes normal incidence, equal-amplitude interface reflections, and a film with n between the surrounding media.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues, {"λ = " + num(in.lambdaNm) + " nm   n = " + num(in.n), "t = " + num(in.thicknessNm) + " nm", "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const double opd = 2 * in.n * in.thicknessNm;
    const double dpsi = 4 * std::numbers::pi * in.n * in.thicknessNm / in.lambdaNm - std::numbers::pi;
    const double refl = std::pow(std::sin(2 * std::numbers::pi * in.n * in.thicknessNm / in.lambdaNm), 2);
    const double firstBright = in.lambdaNm / (4 * in.n);

    if (in.colours) {
        // Third tab: every wavelength at once. The film reflects a comb of wavelengths; the eye sees their sum.
        const FilmColour fc = thinFilmColour(in.thicknessNm, in.n);
        const double vis = thinFilmColourVisibility(in.thicknessNm, in.n);
        ModelReport out;
        Report& rep = out.report;
        LcdRegister a;
        a.title = "FILM COLOUR / λ SWEEP";
        a.lines = {"R(λ) = sin²(2πnt/λ)", "OPD = 2nt = " + num(opd) + " nm", "visibility V = " + NF::fixed(vis, 3) + "  (1 = saturated, 0 = grey)"};
        for (const FilmColourSample& s : fc.lines)
            a.lines.push_back(NF::fixed(s.wavelengthNm, 0) + " nm: " + NF::fixed(s.reflectance, 3));
        a.resultLabel = "PERCEIVED COLOUR";
        a.resultValue = fc.hueNm > 0 ? NF::fixed(fc.hueNm, 0) + " nm dominant" : "neutral grey";
        LcdRegister b;
        b.title = "ORDER COUNT / THICKNESS";
        const double orders = 2 * in.n * in.thicknessNm * (1.0 / 400.0 - 1.0 / 700.0);
        b.lines = {"orders across 400–700 nm:", "Δm = 2nt (1/400 − 1/700) = " + NF::fixed(orders, 3),
                   orders < 1.0 ? "one order: saturated colours" : orders < 3.0 ? "a few orders: pastel colours" : "many orders: grey film",
                   "mean reflectance = " + NF::fixed(fc.meanReflectance, 3), "swatch " + fc.hex};
        b.resultLabel = "SWATCH";
        b.resultValue = fc.hex;
        rep.registers = {a, b};
        rep.notices = {interpretation("A " + num(in.thicknessNm) + " nm film reflects " +
                                      (fc.hueNm > 0 ? NF::fixed(fc.hueNm, 0) + " nm strongest, so it reads as " + fc.lines[4].name +
                                                       ". Thicker films reflect many orders at once and the colours add up to grey — which is why soap films show colour bands only where they are thin."
                                                     : "many wavelengths at once with no clear winner: the film looks grey.")),
                       guard("The swatch is a CIE 1931 integration of R(λ) under a flat illuminant, with equal-amplitude reflections. Real soap films vary in thickness across the surface.")};
        rep.headline = "film " + num(in.thicknessNm) + " nm · " + (fc.hueNm > 0 ? NF::fixed(fc.hueNm, 0) + " nm" : std::string("grey"));
        rep.retained = retained({"t = " + NF::fixed(in.thicknessNm, 2) + " nm · n = " + fx(in.n), "λ range 400–700 nm"}, "RETAINED FILM COLOUR", fc.hex);
        rep.parameters = {{"t", num(in.thicknessNm) + " nm"}, {"n", num(in.n)}, {"swatch", fc.hex}, {"mode", "colours"}};

        ChartData& c = out.chart;
        c.xLabel = "λ (nm)"; c.yLabel = "R(λ)";
        c.xMin = 400; c.xMax = 700; c.yMin = 0; c.yMax = 1; c.xTicks = 3; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
        for (int k = 0; k <= 400; ++k) {
            const double w = 400 + 300.0 * k / 400.0;
            c.curve.push_back({w, thinFilmReflectance(in.thicknessNm, in.n, w)});
        }
        c.marker = {{in.lambdaNm, thinFilmReflectance(in.thicknessNm, in.n, in.lambdaNm)}};
        c.legend = "R(λ) of the film   ● your λ = " + num(in.lambdaNm) + " nm";
        return out;
    }

    ModelReport out;
    Report& rep = out.report;
    if (in.transmission) {
        const double trans = 1.0 - refl;                            // lossless, equal-amplitude interfaces
        const double firstDark = in.lambdaNm / (4 * in.n);
        LcdRegister a;
        a.title = "OPTICAL PATH / PHASE";
        a.lines = {"OPD = 2nt", " = 2 × " + num(in.n) + " × " + num(in.thicknessNm) + " nm", " = " + num(opd) + " nm",
                   "I_t/I_t,max = cos²(2πnt/λ)"};
        a.resultLabel = "NORMALIZED TRANSMISSION";
        a.resultValue = fx(trans);
        LcdRegister b;
        b.title = "NO NET PHASE REVERSAL";
        b.lines = {"Bright: 2nt = qλ", "Dark: 2nt = (q + ½)λ", "q = 0, 1, 2, …"};
        b.resultLabel = "FIRST DARK THICKNESS";
        b.resultValue = fx(firstDark) + " nm";
        rep.registers = {a, b};
        rep.notices = {interpretation("Both surfaces reverse the phase on transmission, so the transmitted beams leave in phase: 2nt = qλ is bright and 2nt = (q + ½)λ is dark — the exact complement of the reflection tab, I_t = 1 − I_r."),
                       guard(guardText)};
        rep.headline = "I_t/I_t,max = " + fx(trans);
        rep.retained = retained({"λ = " + NF::fixed(in.lambdaNm, 1) + " nm · n = " + fx(in.n), "t = " + NF::fixed(in.thicknessNm, 2) + " nm"},
                                "RETAINED TRANSMISSION", a.resultValue);
        rep.parameters = {{"λ", num(in.lambdaNm) + " nm"}, {"n", num(in.n)}, {"t", num(in.thicknessNm) + " nm"}, {"mode", "transmission"}};

        ChartData& c = out.chart;
        c.xLabel = "t (nm)"; c.yLabel = "I_t / I_t,max";
        c.xMin = 0; c.xMax = std::max(400.0, std::ceil(in.thicknessNm / 100.0) * 100.0); c.yMin = 0; c.yMax = 1;
        c.xTicks = static_cast<int>(c.xMax / 100); c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
        for (int k = 0; k <= 400; ++k) {
            const double t = c.xMax * k / 400.0;
            c.curve.push_back({t, std::pow(std::cos(2 * std::numbers::pi * in.n * t / in.lambdaNm), 2)});
        }
        c.marker = {{in.thicknessNm, trans}};
        c.legend = "I_t = cos²(2πnt/λ)   ● t = " + num(in.thicknessNm) + " nm";
        return out;
    }
    LcdRegister a;
    a.title = "OPTICAL PATH / PHASE";
    a.lines = {"OPD = 2nt", " = 2 × " + num(in.n) + " × " + num(in.thicknessNm) + " nm", " = " + num(opd) + " nm",
               "Δψ = 4πnt/λ − π = " + NF::fixed(std::fmod(dpsi + 1e-12, 2 * std::numbers::pi), 4) + " rad", "I_r/I_r,max = sin²(2πnt/λ)"};
    a.resultLabel = "NORMALIZED REFLECTION";
    a.resultValue = fx(refl);
    LcdRegister b;
    b.title = "ONE PHASE REVERSAL";
    b.lines = {"Bright: 2nt = (q + ½)λ", "Dark: 2nt = qλ", "q = 0, 1, 2, …"};
    b.resultLabel = "FIRST BRIGHT THICKNESS";
    b.resultValue = fx(firstBright) + " nm";
    rep.registers = {a, b};
    rep.notices = {interpretation("With one reflection phase reversal, thickness λ/(4n) gives the first bright reflection; thickness λ/(2n) returns to dark."), guard(guardText)};
    rep.headline = "I_r/I_r,max = " + fx(refl);
    rep.retained = retained({"λ = " + NF::fixed(in.lambdaNm, 1) + " nm · n = " + fx(in.n), "t = " + NF::fixed(in.thicknessNm, 2) + " nm"}, "RETAINED REFLECTION", a.resultValue);
    rep.parameters = {{"λ", num(in.lambdaNm) + " nm"}, {"n", num(in.n)}, {"t", num(in.thicknessNm) + " nm"}, {"mode", "reflection"}};

    ChartData& c = out.chart;
    c.xLabel = "t (nm)"; c.yLabel = "I_r / I_r,max";
    c.xMin = 0; c.xMax = std::max(400.0, std::ceil(in.thicknessNm / 100.0) * 100.0); c.yMin = 0; c.yMax = 1;
    c.xTicks = static_cast<int>(c.xMax / 100); c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
    for (int k = 0; k <= 400; ++k) {
        const double t = c.xMax * k / 400.0;
        c.curve.push_back({t, std::pow(std::sin(2 * std::numbers::pi * in.n * t / in.lambdaNm), 2)});
    }
    c.marker = {{in.thicknessNm, refl}};
    c.legend = "I_r = sin²(2πnt/λ)   ● t = " + num(in.thicknessNm) + " nm";
    return out;
}

// ================================================================================ diffraction grating
std::vector<int> allowedGratingOrders(double linesPerCm, double lambdaNm, double incidenceDeg) {
    std::vector<int> orders;
    if (linesPerCm <= 0 || lambdaNm <= 0) return orders;
    const double d = 0.01 / linesPerCm, lam = lambdaNm * 1e-9, si = std::sin(incidenceDeg * kDeg);
    const int limit = static_cast<int>(std::floor(2.0 * d / lam)) + 1;
    for (int m = -limit; m <= limit; ++m)
        if (std::fabs(m * lam / d + si) <= 1.0 + 1e-12) orders.push_back(m);
    return orders;
}

double gratingIntensity(double thetaDeg, double linesPerCm, double lambdaNm, double slits, double incidenceDeg) {
    const double d = 0.01 / linesPerCm, lam = lambdaNm * 1e-9;
    const double phi = 2 * std::numbers::pi * d * (std::sin(thetaDeg * kDeg) - std::sin(incidenceDeg * kDeg)) / lam;
    const double den = slits * std::sin(phi / 2);
    if (std::fabs(den) < 1e-9) return 1.0;                          // principal maximum
    const double v = std::sin(slits * phi / 2) / den;
    return v * v;
}

ModelReport grating(const GratingInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.linesPerCm <= 0) issues.push_back({FieldIds::gratingDensity, "Enter N > 0 lines/cm.", "N"});
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.slits < 2 || !isInteger(in.slits)) issues.push_back({FieldIds::illuminatedSlits, "Enter an integer M ≥ 2.", "M"});
    if (!isInteger(in.order)) issues.push_back({FieldIds::order, "Enter an integer order m.", "m"});
    const std::string guardText = "Require N, λ > 0 and integer M ≥ 2. Reject selected m if |mλ/d| > 1; never evaluate asin outside its domain.";
    if (issues.empty()) {
        const double d = 0.01 / in.linesPerCm, lam = in.lambdaNm * 1e-9;
        const double s = in.order * lam / d + std::sin(in.incidenceDeg * kDeg);
        if (std::fabs(s) > 1.0) issues.push_back({FieldIds::order, "No real angle: |mλ/d| > 1.", "m"});
    }
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues, {"N = " + num(in.linesPerCm) + " lines/cm   λ = " + num(in.lambdaNm) + " nm", "m = " + num(in.order) + "   M = " + num(in.slits), "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const int m = static_cast<int>(std::lround(in.order));
    const double d = 0.01 / in.linesPerCm, lam = in.lambdaNm * 1e-9;
    const double sinTheta = m * lam / d + std::sin(in.incidenceDeg * kDeg);
    const double angle = std::asin(sinTheta) / kDeg;
    const auto orders = allowedGratingOrders(in.linesPerCm, in.lambdaNm, in.incidenceDeg);

    ModelReport out;
    Report& rep = out.report;
    if (in.orderList) {
        // Second tab: the order table — every propagating order with the angle it lands at.
        const int capacity = static_cast<int>(std::floor(d / lam + 1e-9));
        auto angleOf = [&](int k) { return std::asin(std::clamp(k * lam / d + std::sin(in.incidenceDeg * kDeg), -1.0, 1.0)) / kDeg; };
        std::vector<std::string> table{"m = 0 at " + NF::fixed(angleOf(0), 2) + "°"};
        for (int k = 1; k <= std::min(capacity, 3); ++k)
            table.push_back("±" + std::to_string(k) + " at ±" + NF::fixed(angleOf(k), 2) + "°");
        table.push_back(capacity > 3 ? "… up to ±" + std::to_string(capacity) + " at ±" + NF::fixed(angleOf(capacity), 2) + "°"
                                     : "no real angle beyond ±" + std::to_string(std::max(0, capacity)));
        LcdRegister a;
        a.title = "ORDER TABLE";
        a.lines = table;
        a.resultLabel = "TOTAL ALLOWED PEAKS";
        a.resultValue = std::to_string(orders.size()) + " orders";
        LcdRegister b;
        b.title = "ANGULAR SEPARATION";
        b.lines = {"Δθ(±1) = 2θ₁ = " + NF::fixed(2 * angleOf(1), 2) + "°",
                   "θ₂ − θ₁ = " + NF::fixed(angleOf(2) - angleOf(1), 2) + "°",
                   "the angular step widens with |m|", "d/λ = " + fx(d / lam)};
        b.resultLabel = "ORDER CAPACITY · d/λ";
        b.resultValue = fx(d / lam);
        rep.registers = {a, b};
        rep.notices = {interpretation("Every order sits where d sin θₘ = mλ: the step in sin θ is the constant λ/d, so the angles widen as |m| grows — θ₂ − θ₁ = " + NF::fixed(angleOf(2) - angleOf(1), 2) + "° against θ₁ = " + NF::fixed(angleOf(1), 2) + "°. d/λ = " + fx(d / lam) + " fixes how many orders can propagate at all."),
                       guard(guardText)};
        rep.headline = std::to_string(orders.size()) + " orders inside ±" + NF::fixed(capacity > 0 ? angleOf(capacity) : 0.0, 2) + "°";
        rep.retained = retained({"N = " + num(in.linesPerCm) + " lines/cm · λ = " + NF::fixed(in.lambdaNm, 1) + " nm",
                                 "M = " + num(in.slits) + " · θᵢ = " + num(in.incidenceDeg) + "°"}, "RETAINED ORDER COUNT", a.resultValue);
        rep.parameters = {{"N", num(in.linesPerCm) + " lines/cm"}, {"λ", num(in.lambdaNm) + " nm"}, {"m", std::to_string(m)},
                          {"M", num(in.slits)}, {"θᵢ", num(in.incidenceDeg) + " °"}, {"mode", "orders"}};

        ChartData& c = out.chart;
        c.xLabel = "θ (degrees)"; c.yLabel = "I / Imax";
        c.xMin = -90; c.xMax = 90; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
        for (int k = 0; k <= 1800; ++k) {
            const double th = -90 + k * 0.1;
            c.curve.push_back({th, gratingIntensity(th, in.linesPerCm, in.lambdaNm, in.slits, in.incidenceDeg)});
        }
        for (int mm : orders) c.marker.push_back({angleOf(mm), 1.0});
        c.legend = std::to_string(orders.size()) + " principal maxima · every peak marked ●";
        return out;
    }
    LcdRegister a;
    a.title = "DENSITY + SPACING / SI";
    a.lines = {"d = 0.01 / N", " = 0.01 / " + num(in.linesPerCm) + " m", " = " + NF::scientific(d, 4) + " m",
               in.incidenceDeg == 0 ? "d sin θₘ = mλ" : "d (sin θₘ − sin θᵢ) = mλ",
               "sin θ" + sub(std::abs(m)) + " = " + num(std::fabs(m) * in.lambdaNm) + " nm / " + num(d * 1e9) + " nm"};
    a.resultLabel = std::string("ORDER ") + (m > 0 ? "+" : "") + std::to_string(m) + " ANGLE";
    a.resultValue = (m == 0 ? fx(0.0) : NF::signedFixed(angle)) + "°";
    LcdRegister b;
    b.title = "PROPAGATING ORDERS";
    std::string list;
    for (std::size_t i = 0; i < orders.size(); ++i) list += (i ? ", " : "") + std::to_string(orders[i]);
    b.lines = {"|m| ≤ floor(d/λ) = " + std::to_string(static_cast<int>(std::floor(d / lam + 1e-9))), "m = " + list};
    b.resultLabel = "TOTAL ALLOWED PEAKS";
    b.resultValue = std::to_string(orders.size()) + " orders";
    rep.registers = {a, b};
    std::string angles;
    for (int k = 1; k <= static_cast<int>(std::floor(d / lam + 1e-9)); ++k)
        angles += (k > 1 ? ", " : "") + std::string(k == 1 ? "±" : "±") + std::to_string(k) + " at ±" + NF::fixed(std::asin(k * lam / d) / kDeg, 2) + "°";
    rep.notices = {interpretation("The zero order is undeviated. Order angles: " + angles + "; the next order has no real angle."), guard(guardText)};
    rep.headline = "θ" + std::string(m > 0 ? "₊" : "") + sub(std::abs(m)) + " = " + a.resultValue;
    rep.retained = retained({"N = " + num(in.linesPerCm) + " lines/cm · λ = " + NF::fixed(in.lambdaNm, 1) + " nm", "m = " + std::to_string(m) + " · M = " + num(in.slits)}, "RETAINED ORDER ANGLE", a.resultValue);
    rep.parameters = {{"N", num(in.linesPerCm) + " lines/cm"}, {"λ", num(in.lambdaNm) + " nm"}, {"m", std::to_string(m)}, {"M", num(in.slits)},
                      {"θᵢ", num(in.incidenceDeg) + " °"}, {"mode", "transmission"}};

    ChartData& c = out.chart;
    c.xLabel = "θ (degrees)"; c.yLabel = "I / Imax";
    c.xMin = -90; c.xMax = 90; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
    for (int k = 0; k <= 1800; ++k) {
        const double th = -90 + k * 0.1;
        c.curve.push_back({th, gratingIntensity(th, in.linesPerCm, in.lambdaNm, in.slits, in.incidenceDeg)});
    }
    c.marker = {{angle, 1.0}};
    c.legend = "M = " + num(in.slits) + " slits · no envelope   ● order " + std::string(m > 0 ? "+" : "") + std::to_string(m);
    return out;
}

// ================================================================================ single slit
double singleSlitIntensity(double thetaDeg, double widthMm, double lambdaNm) {
    const double beta = std::numbers::pi * widthMm * 1e-3 * std::sin(thetaDeg * kDeg) / (lambdaNm * 1e-9);
    return std::fabs(beta) < 1e-9 ? 1.0 : std::pow(std::sin(beta) / beta, 2);
}

ModelReport singleSlit(const SingleSlitInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.widthMm <= 0) issues.push_back({FieldIds::slitWidth, "Enter a > 0 mm.", "a"});
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.screenM <= 0) issues.push_back({FieldIds::screenDistance, "Enter L > 0 m.", "L"});
    if (issues.empty() && in.lambdaNm * 1e-9 / (in.widthMm * 1e-3) > 1.0) issues.push_back({FieldIds::slitWidth, "No first minimum: λ/a > 1.", "a"});
    const std::string guardText = "At β = 0 use the limit I/I₀ = 1, not 0/0. A real first minimum needs λ/a ≤ 1.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues, {"a = " + NF::fixed(in.widthMm, 4) + " mm   λ = " + num(in.lambdaNm) + " nm", "L = " + num(in.screenM) + " m", "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const double a = in.widthMm * 1e-3, lam = in.lambdaNm * 1e-9;
    const double s1 = lam / a;
    const double theta = std::asin(s1) / kDeg;
    const double w = 2 * in.screenM * std::tan(theta * kDeg);

    ModelReport out;
    Report& rep = out.report;
    if (in.geometry) {
        // Second tab: the screen itself — where each minimum lands, not the intensity envelope.
        auto yAt = [&](int k) { return in.screenM * std::tan(std::asin(std::min(1.0, k * lam / a))); };
        const double y1 = yAt(1), y2 = yAt(2), y3 = yAt(3);
        LcdRegister r1;
        r1.title = "MINIMA ON THE SCREEN";
        r1.lines = {"yₘ = L tan θₘ,  sin θₘ = mλ/a",
                    "y₁ = ±" + fx(y1 * 1e3) + " mm", "y₂ = ±" + fx(y2 * 1e3) + " mm", "y₃ = ±" + fx(y3 * 1e3) + " mm"};
        r1.resultLabel = "SCREEN POSITION · y₁";
        r1.resultValue = fx(y1 * 1e3) + " mm";
        LcdRegister r2;
        r2.title = "CENTRAL LOBE / SIDE LOBE";
        r2.lines = {"w = 2L tan θ₁ = " + fx(w * 1e3) + " mm", "w = 2 y₁", "the first side maximum sits at 1.43 θ₁"};
        r2.resultLabel = "FIRST SIDE-MAXIMUM POSITION";
        r2.resultValue = fx(1.43 * y1 * 1e3) + " mm";
        rep.registers = {r1, r2};
        rep.notices = {interpretation("Dark fringes are almost evenly spaced on the screen: y₁ = " + fx(y1 * 1e3) + " mm, y₂ − y₁ = " + fx((y2 - y1) * 1e3) + " mm. The central lobe is twice as wide as a side lobe, and the side maxima sit at 1.43 θ₁, 2.46 θ₁, …"),
                       guard(guardText)};
        rep.headline = "y₁ = ±" + fx(y1 * 1e3) + " mm";
        rep.retained = retained({"a = " + NF::fixed(in.widthMm, 4) + " mm · λ = " + NF::fixed(in.lambdaNm, 1) + " nm",
                                 "L = " + NF::fixed(in.screenM, 4) + " m"}, "RETAINED FIRST MINIMUM POSITION", r1.resultValue);
        rep.parameters = {{"a", num(in.widthMm) + " mm"}, {"λ", num(in.lambdaNm) + " nm"}, {"L", num(in.screenM) + " m"}, {"mode", "geometry"}};

        ChartData& c = out.chart;
        c.xLabel = "m (order)"; c.yLabel = "yₘ (mm)";
        c.xMin = 0; c.xMax = 5; c.yMin = 0; c.yMax = yAt(5) * 1e3; c.xTicks = 5; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
        for (int k = 0; k <= 5; ++k) c.curve.push_back({double(k), k == 0 ? 0.0 : yAt(k) * 1e3});
        c.marker = {{1.0, y1 * 1e3}};
        c.legend = "yₘ vs order · yₘ ≈ m y₁   ● first minimum";
        return out;
    }
    LcdRegister r1;
    r1.title = "FIRST MINIMUM / DEG";
    r1.lines = {"a sin θ₁ = λ", "sin θ₁ = " + NF::scientificCompact(lam) + " / " + NF::scientificCompact(a), "θ₁ = asin(" + NF::general(s1, 6) + ")"};
    r1.resultLabel = "FIRST DARK ANGLE · θ₁";
    r1.resultValue = fx(theta) + "°";
    LcdRegister r2;
    r2.title = "CENTRAL MAXIMUM";
    r2.lines = {"w = 2L tan θ₁", " = 2 × " + num(in.screenM) + " × tan(" + fx(theta) + "°)", " = " + NF::fixed(w, 9) + " m"};
    r2.resultLabel = "FULL WIDTH ON SCREEN";
    r2.resultValue = fx(w * 1e3) + " mm";
    rep.registers = {r1, r2};
    rep.notices = {interpretation("The central lobe spans the first minima at ±" + fx(w * 1e3 / 2) + " mm. Side lobes diminish; the first side maximum is only about 4.7% of I₀."), guard(guardText)};
    rep.headline = "θ₁ = " + fx(theta) + "°";
    rep.retained = retained({"a = " + NF::fixed(in.widthMm, 4) + " mm · λ = " + NF::fixed(in.lambdaNm, 1) + " nm", "L = " + NF::fixed(in.screenM, 4) + " m"}, "RETAINED FIRST DARK ANGLE", r1.resultValue);
    rep.parameters = {{"a", num(in.widthMm) + " mm"}, {"λ", num(in.lambdaNm) + " nm"}, {"L", num(in.screenM) + " m"}, {"mode", "intensity"}};

    ChartData& c = out.chart;
    c.xLabel = "θ (degrees)"; c.yLabel = "I / I₀";
    const double range = std::max(0.3, std::round(4.2 * theta * 10) / 10);
    c.xMin = -range; c.xMax = range; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 1; c.yDecimals = 1;
    for (int k = 0; k <= 800; ++k) {
        const double th = -range + 2 * range * k / 800.0;
        c.curve.push_back({th, singleSlitIntensity(th, in.widthMm, in.lambdaNm)});
    }
    c.marker = {{theta, 0.0}};
    c.legend = "(sin β / β)², β = π a sin θ / λ   ● first minimum";
    return out;
}

// ================================================================================ Malus' law and Brewster's angle
ModelReport polarization(const PolarizationInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.incidentIntensity < 0) issues.push_back({FieldIds::incidentIntensity, "Enter Iᵢₙ ≥ 0.", "Iᵢₙ"});
    if (in.angleDeg < 0 || in.angleDeg > 90) issues.push_back({FieldIds::relativeAngle, "Enter 0° ≤ θ ≤ 90°.", "θ"});
    if (in.n1 <= 0) issues.push_back({FieldIds::refractiveIndex1, "Enter n₁ > 0.", "n₁"});
    if (in.n2 <= 0) issues.push_back({FieldIds::refractiveIndex2, "Enter n₂ > 0.", "n₂"});
    const std::string guardText = "Require Iᵢₙ ≥ 0 and n₁, n₂ > 0. Brewster assumes transparent, nonmagnetic media.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues, {"Iᵢₙ = " + num(in.incidentIntensity) + " W/m²   θ = " + num(in.angleDeg) + "°", "n₁ = " + num(in.n1) + "   n₂ = " + num(in.n2), "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const double i0 = in.unpolarized ? in.incidentIntensity / 2 : in.incidentIntensity;
    const double c2 = std::pow(std::cos(in.angleDeg * kDeg), 2);
    const double out2 = i0 * c2;
    const double brewster = std::atan(in.n2 / in.n1) / kDeg;
    auto medium = [](double n) { return std::fabs(n - 1.0) < 1e-6 ? "AIR" : std::fabs(n - 1.5) < 1e-6 ? "GLASS" : std::fabs(n - 1.333) < 1e-3 ? "WATER" : "MEDIUM"; };

    ModelReport out;
    Report& rep = out.report;
    if (in.brewster) {
        // Second tab: the interface at Brewster, with Fresnel reflectances instead of Malus' law.
        auto fresnel = [&](double th, double& rs, double& rp) {           // unpolarized light, n₁ → n₂
            const double s2 = (in.n1 / in.n2) * std::sin(th);
            const double ct = std::sqrt(std::max(0.0, 1 - s2 * s2));
            const double c = std::cos(th);
            const double as = (in.n1 * c - in.n2 * ct) / (in.n1 * c + in.n2 * ct);
            const double ap = (in.n2 * c - in.n1 * ct) / (in.n2 * c + in.n1 * ct);
            rs = as * as;
            rp = ap * ap;
        };
        const double thB = std::atan(in.n2 / in.n1);
        const double thR = std::numbers::pi / 2 - thB;
        double rsB = 0, rpB = 0;
        fresnel(thB, rsB, rpB);
        const double r0 = std::pow((in.n1 - in.n2) / (in.n1 + in.n2), 2);
        LcdRegister a;
        a.title = std::string("BREWSTER / ") + medium(in.n1) + " → " + medium(in.n2);
        a.lines = {"tan θB = n₂ / n₁ = " + NF::fixed(in.n2, 4) + " / " + NF::fixed(in.n1, 4),
                   "θB = atan(" + NF::fixed(in.n2 / in.n1, 6) + ") = " + fx(brewster) + "°",
                   "θr = 90° − θB = " + fx(thR / kDeg) + "°",
                   "reflected and refracted rays are perpendicular"};
        a.resultLabel = "INCIDENCE FROM NORMAL";
        a.resultValue = fx(brewster) + "°";
        LcdRegister b;
        b.title = "FRESNEL AT θB";
        b.lines = {"R_p = " + fx(rpB) + "   (p-polarized, no reflection)",
                   "R_s = " + fx(rsB) + "   (s-polarized)",
                   "R_unpolarized = (R_s + R_p)/2 = " + fx((rsB + rpB) / 2),
                   "at normal incidence R = " + fx(r0)};
        b.resultLabel = "s-POLARIZED REFLECTANCE · Rs";
        b.resultValue = fx(rsB);
        rep.registers = {a, b};
        rep.notices = {interpretation("At θB the reflected beam is purely s-polarized: R_p falls to " + fx(rpB) + " while R_s stays at " + fx(rsB) + ", so " + NF::general((rsB + rpB) / 2 * 100, 3) + "% of an unpolarized beam is reflected. The refracted ray leaves at 90° − θB = " + fx(thR / kDeg) + "°."),
                       guard(guardText)};
        rep.headline = "θB = " + fx(brewster) + "° · R_p = " + fx(rpB);
        rep.retained = retained({"n₁ = " + num(in.n1) + " · n₂ = " + num(in.n2), "Iᵢₙ = " + NF::fixed(in.incidentIntensity, 2) + " W/m²"},
                                "RETAINED BREWSTER ANGLE", a.resultValue);
        rep.parameters = {{"n₁", num(in.n1)}, {"n₂", num(in.n2)}, {"Iᵢₙ", num(in.incidentIntensity) + " W/m²"}, {"mode", "brewster"}};

        ChartData& c = out.chart;
        c.xLabel = "θ (degrees)"; c.yLabel = "R_p";
        c.xMin = 0; c.xMax = 90; c.yMin = 0; c.yMax = 1; c.xTicks = 3; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 1;
        for (int k = 0; k <= 180; ++k) {
            double rs = 0, rp = 0;
            fresnel(k * 0.5 * kDeg, rs, rp);
            c.curve.push_back({k * 0.5, rp});
        }
        c.marker = {{brewster, rpB}};
        c.legend = "R_p(θ) reaches 0 at θB = " + fx(brewster) + "°   ● Brewster";
        return out;
    }
    LcdRegister a;
    a.title = "MALUS / DEG";
    a.lines = {in.unpolarized ? "I₀ = Iᵢₙ / 2 = " + num(i0) + " W/m²" : "I₀ = Iᵢₙ = " + num(i0) + " W/m²", "I = I₀ cos²θ", "  = " + num(i0) + " × cos²(" + num(in.angleDeg) + "°)", "  = " + num(i0) + " × " + fx(c2)};
    a.resultLabel = "ANALYZER OUTPUT";
    a.resultValue = fx(out2) + " W/m²";
    LcdRegister b;
    b.title = std::string("BREWSTER / ") + medium(in.n1) + " → " + medium(in.n2);
    b.lines = {"tan θB = n₂ / n₁", "θB = atan(" + NF::fixed(in.n2, 1) + " / " + NF::fixed(in.n1, 1) + ")"};
    b.resultLabel = "INCIDENCE FROM NORMAL";
    b.resultValue = fx(brewster) + "°";
    rep.registers = {a, b};
    const double passed = c2 * 100, ofOriginal = in.incidentIntensity > 0 ? out2 / in.incidentIntensity * 100 : 0;
    rep.notices = {interpretation("The analyzer passes " + NF::general(passed, 4) + "% of I₀, or " + NF::general(ofOriginal, 4) + "% of the original beam. At θB the reflected beam is purely s-polarized for an ideal dielectric."), guard(guardText)};
    rep.headline = "I = " + fx(out2) + " W/m²";
    rep.retained = retained({"Iᵢₙ = " + NF::fixed(in.incidentIntensity, 2) + " W/m² · θ = " + NF::fixed(in.angleDeg, 2) + "°"}, "RETAINED ANALYZER OUTPUT", a.resultValue);
    rep.parameters = {{"Iᵢₙ", num(in.incidentIntensity) + " W/m²"}, {"θ", num(in.angleDeg) + " °"}, {"n₁", num(in.n1)}, {"n₂", num(in.n2)},
                      {"Beam", in.unpolarized ? "Unpolarized" : "Polarized"}, {"mode", "malus"}};

    ChartData& c = out.chart;
    c.xLabel = "θ (degrees)"; c.yLabel = "I (W/m²)";
    c.xMin = 0; c.xMax = 90; c.yMin = 0; c.yMax = std::max(1.0, i0); c.xTicks = 3; c.yTicks = 2; c.xDecimals = 0; c.yDecimals = 0;
    for (int k = 0; k <= 180; ++k) c.curve.push_back({k * 0.5, i0 * std::pow(std::cos(k * 0.5 * kDeg), 2)});
    c.marker = {{in.angleDeg, out2}};
    c.legend = "Malus: I = I₀ cos²θ   ● θ = " + num(in.angleDeg) + "°";
    return out;
}

// -------------------------------------------------------------------------------- single slit: envelope
double multiSlitFactor(double phaseStepRad, double sources) {
    const double half = phaseStepRad / 2;
    const double den = sources * std::sin(half);
    if (std::fabs(den) < 1e-9) return 1.0;                       // principal maximum: the limit is 1
    const double v = std::sin(sources * half) / den;
    return v * v;
}

double doubleSlitEnvelopeIntensity(double thetaDeg, double widthMm, double separationMm, double lambdaNm) {
    const double sinT = std::sin(thetaDeg * kDeg);
    const double lam = lambdaNm * 1e-9;
    const double beta = std::numbers::pi * widthMm * 1e-3 * sinT / lam;
    const double slit = std::fabs(beta) < 1e-9 ? 1.0 : std::pow(std::sin(beta) / beta, 2);
    const double phi = 2 * std::numbers::pi * separationMm * 1e-3 * sinT / lam;   // phase between the two slits
    return slit * std::pow(std::cos(phi / 2), 2);
}

ModelReport doubleSlitEnvelope(const SingleSlitInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.widthMm <= 0) issues.push_back({FieldIds::slitWidth, "Enter a > 0 mm.", "a"});
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.screenM <= 0) issues.push_back({FieldIds::screenDistance, "Enter L > 0 m.", "L"});
    if (in.separationMm <= 0) issues.push_back({FieldIds::slitSeparation, "Enter d > 0 mm.", "d"});
    if (issues.empty() && in.separationMm < in.widthMm) issues.push_back({FieldIds::slitSeparation, "The slit separation must exceed the slit width.", "d"});
    const std::string guardText = "Two identical slits of finite width a, separation d > a, equal amplitudes and L large. Missing fringes appear where d/a is an integer.";
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues,
                      {"a = " + num(in.widthMm) + " mm   λ = " + num(in.lambdaNm) + " nm", "d = " + num(in.separationMm) + " mm   L = " + num(in.screenM) + " m",
                       "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const double a = in.widthMm * 1e-3, d = in.separationMm * 1e-3, lam = in.lambdaNm * 1e-9;
    const double s1 = lam / a, theta1 = std::asin(std::min(1.0, s1)) / kDeg;      // first missing order (slit minimum)
    const double fringe = lam * in.screenM / d;                                   // fringe spacing on the screen (m)
    const double width = 2 * in.screenM * std::tan(theta1 * kDeg);                // envelope width (m)
    const double ratio = d / a;
    const bool missing = std::fabs(ratio - std::round(ratio)) < 1e-9 && std::round(ratio) >= 1;

    ModelReport out;
    Report& rep = out.report;
    LcdRegister r1;
    r1.title = "TWO SLITS OF WIDTH a / DEG";
    r1.lines = {"I = I₀ cos²(πd sinθ/λ) · (sinβ/β)²", "β = π a sin θ / λ",
                "first slit minimum θ = " + fx(theta1) + "°", "fringe spacing Δθ = " + fx(lam / d / kDeg) + "°"};
    r1.resultLabel = "ENVELOPE HALF-WIDTH · θ₁";
    r1.resultValue = fx(theta1) + "°";
    LcdRegister r2;
    r2.title = "MISSING ORDERS / d ÷ a";
    r2.lines = {"d/a = " + fx(ratio), "fringe m sits at sin θ = mλ/d", "slit minimum at sin θ = λ/a",
                missing ? "m = " + std::to_string(static_cast<int>(std::lround(ratio))) + " falls on the first slit minimum"
                        : "no fringe coincides with the first slit minimum"};
    r2.resultLabel = "FRINGES INSIDE THE ENVELOPE";
    r2.resultValue = std::to_string(std::max(1, static_cast<int>(2 * std::floor(ratio)) - 1)) + " bright";
    rep.registers = {r1, r2};
    rep.notices = {interpretation("The slit width sets the envelope and the separation sets the fine fringes: " +
                                  std::to_string(static_cast<int>(std::lround(width / fringe))) + " fringes fit inside the central envelope" +
                                  (missing ? ", and the fringe at m = " + std::to_string(static_cast<int>(std::lround(ratio))) + " is missing because it lands on the first slit minimum." : ".")),
                   guard(guardText)};
    rep.headline = "θ_env = " + fx(theta1) + "°";
    rep.retained = retained({"a = " + NF::fixed(in.widthMm, 4) + " mm · d = " + NF::fixed(in.separationMm, 4) + " mm", "λ = " + NF::fixed(in.lambdaNm, 1) + " nm · L = " + NF::fixed(in.screenM, 4) + " m"},
                            "RETAINED ENVELOPE WIDTH", fx(theta1) + "°");
    rep.parameters = {{"a", num(in.widthMm) + " mm"}, {"d", num(in.separationMm) + " mm"}, {"λ", num(in.lambdaNm) + " nm"},
                      {"L", num(in.screenM) + " m"}, {"mode", "envelope"}};

    ChartData& c = out.chart;
    c.xLabel = "θ (degrees)"; c.yLabel = "I / I₀";
    const double range = std::max(0.6, std::round(3.2 * theta1 * 10) / 10);
    c.xMin = -range; c.xMax = range; c.yMin = 0; c.yMax = 1; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 1; c.yDecimals = 1;
    for (int k = 0; k <= 1400; ++k) {
        const double th = -range + 2 * range * k / 1400.0;
        c.curve.push_back({th, doubleSlitEnvelopeIntensity(th, in.widthMm, in.separationMm, in.lambdaNm)});
    }
    c.marker = {{theta1, 0.0}};
    c.legend = "cos² fringe × sinc² envelope   ● first slit minimum";
    return out;
}

// -------------------------------------------------------------------------------- grating: resolving power
ModelReport gratingResolution(const GratingResolutionInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (in.linesPerCm <= 0) issues.push_back({FieldIds::gratingDensity, "Enter N > 0 lines/cm.", "N"});
    if (in.lambdaNm <= 0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (in.order < 1 || !isInteger(in.order)) issues.push_back({FieldIds::order, "Enter an integer order m ≥ 1.", "m"});
    if (in.slits < 2) issues.push_back({FieldIds::illuminatedSlits, "Enter M ≥ 2 illuminated lines.", "M"});
    const std::string guardText = "Resolving power uses the whole illuminated width: with B the illuminated width, N_used = B/d and R = mN_used. Rayleigh separation δλ = λ / R.";
    if (issues.empty()) {
        const double d = 0.01 / in.linesPerCm, lam = in.lambdaNm * 1e-9;
        if (in.order * lam / d > 1.0) issues.push_back({FieldIds::order, "This order does not propagate: |mλ/d| > 1.", "m"});
    }
    if (!issues.empty())
        return paused(issues.front().label + " out of range", issues,
                      {"N = " + num(in.linesPerCm) + " lines/cm   λ = " + num(in.lambdaNm) + " nm", "m = " + num(in.order) + "   M = " + num(in.slits),
                       "ACTION: " + issues.front().message},
                      guardText + "\n\nDiagram, chart and retained results show the last valid run.", ctx);

    const int m = static_cast<int>(std::lround(in.order));
    const double d = 0.01 / in.linesPerCm, lam = in.lambdaNm * 1e-9;
    const double angle = std::asin(std::min(1.0, m * lam / d)) / kDeg;
    const double R = m * in.slits;                                               // resolving power
    const double dLambda = in.lambdaNm / R;                                      // smallest separable gap (nm)
    const double fsr = in.lambdaNm / m;                                          // free spectral range (nm)
    const double width = d * in.slits;                                           // illuminated width (m)

    ModelReport out;
    Report& rep = out.report;
    LcdRegister r1;
    r1.title = "RESOLVING POWER / R = mN";
    r1.lines = {"R = |m| × N_used", "  = " + std::to_string(std::abs(m)) + " × " + num(in.slits), "  = " + NF::general(R, 6),
                "δλ = λ / R", "δλ = " + num(in.lambdaNm) + " nm / " + NF::general(R, 6)};
    r1.resultLabel = "RAYLEIGH LIMIT · δλ";
    r1.resultValue = NF::general(dLambda, 3) + " nm";
    LcdRegister r2;
    r2.title = "FREE SPECTRAL RANGE / m";
    r2.lines = {"FSR = λ / m", "FSR = " + num(fsr) + " nm", "λ + FSR would overlap in order m",
                "illuminated width N·d = " + NF::fixed(width * 1e3, 3) + " mm", "order " + std::to_string(m) + " angle = " + fx(angle) + "°"};
    r2.resultLabel = "ORDER-OVERLAP LIMIT";
    r2.resultValue = num(fsr) + " nm";
    rep.registers = {r1, r2};
    rep.notices = {interpretation("With " + num(in.slits) + " illuminated lines the grating separates two wavelengths " + NF::general(dLambda, 3) +
                                  " nm apart in order " + std::to_string(m) + " — doubling the illuminated width halves that gap. The same order repeats every " +
                                  NF::general(fsr, 4) + " nm, so a spectrum is unambiguous only inside one free spectral range."),
                   guard(guardText)};
    rep.headline = "δλ = " + NF::general(dLambda, 3) + " nm";
    rep.retained = retained({"N = " + num(in.linesPerCm) + " lines/cm · m = " + std::to_string(m), "M = " + num(in.slits) + " lines · λ = " + NF::fixed(in.lambdaNm, 2) + " nm"},
                            "RETAINED RESOLUTION", NF::general(dLambda, 3) + " nm");
    rep.parameters = {{"N", num(in.linesPerCm) + " lines/cm"}, {"λ", num(in.lambdaNm) + " nm"}, {"m", std::to_string(m)},
                      {"M", num(in.slits)}, {"R", NF::general(R, 6)}, {"δλ", NF::general(dLambda, 3) + " nm"}, {"mode", "resolution"}};

    // Rayleigh pair: two lines δλ apart at order m, unresolved -> just resolved -> clearly split
    ChartData& c = out.chart;
    c.xLabel = "wavelength (nm)"; c.yLabel = "I / Imax";
    const double centre = in.lambdaNm, half = 2.5 * dLambda;
    c.xMin = centre - half; c.xMax = centre + half; c.yMin = 0; c.yMax = 1.05; c.xTicks = 4; c.yTicks = 2; c.xDecimals = 2; c.yDecimals = 1;
    const auto profile = [&](double lambdaNm) {                                 // the order-m peak shape, in wavelength
        const double x = (lambdaNm - centre) / dLambda;                          // 1 = one Rayleigh step away
        return multiSlitFactor(2 * std::numbers::pi * x / (R / std::max(1.0, std::abs(double(m)))), in.slits);
    };
    for (int k = 0; k <= 600; ++k) {
        const double w = c.xMin + (c.xMax - c.xMin) * k / 600.0;
        c.curve.push_back({w, profile(w)});
    }
    for (int k = 0; k <= 600; ++k) {
        const double w = c.xMin + (c.xMax - c.xMin) * k / 600.0;
        c.marker.push_back({w, 0.5 * (profile(w - 0.5 * dLambda) + profile(w + 0.5 * dLambda))});
    }
    c.legend = "single line ▬ and a pair δλ apart ● · δλ = " + NF::general(dLambda, 3) + " nm";
    return out;
}

} // namespace phy104::WaveExperiments

namespace phy104 {
// -------------------------------------------------------------------------------- thin film: colours

double thinFilmReflectance(double thicknessNm, double n, double wavelengthNm) {
    if (wavelengthNm <= 0) return 0.0;
    return std::pow(std::sin(2 * std::numbers::pi * n * thicknessNm / wavelengthNm), 2);
}

double thinFilmColourVisibility(double thicknessNm, double n) {
    const double opd = 2 * n * thicknessNm;
    if (opd <= 0) return 0.0;
    const double orders = opd * (1.0 / 400.0 - 1.0 / 700.0);          // how many orders fit across the visible band
    return std::exp(-orders * orders / 2.2);                         // 1 for a thin film, -> 0 for a thick one
}

FilmColour thinFilmColour(double thicknessNm, double n) {
    FilmColour out;
    const auto refl = [thicknessNm, n](double w) { return thinFilmReflectance(thicknessNm, n, w); };
    const Rgb rgb = Spectrum::reflectanceToRgb(refl);
    const double vis = thinFilmColourVisibility(thicknessNm, n);
    // dominant hue: the strongest reflected line (a washed-out film has none -> neutral)
    double best = -1, hue = 0, mean = 0;
    for (int w = 400; w <= 700; w += 10) {
        const double r = refl(w);
        mean += r;
        if (r > best) { best = r; hue = w; }
    }
    mean /= 31.0;
    out.meanReflectance = mean;
    out.hueNm = vis > 0.35 ? hue : 0.0;
    const double lightness = std::clamp(0.45 + 0.55 * mean, 0.0, 1.0);  // many orders add up to a pale film
    static const char* names[] = {"Violet", "Blue", "Cyan", "Green", "Yellow", "Orange", "Red"};
    const auto describe = [](double w) {
        if (w <= 0) return std::string("Neutral grey");
        const int band = w < 430 ? 0 : w < 480 ? 1 : w < 510 ? 2 : w < 560 ? 3 : w < 590 ? 4 : w < 620 ? 5 : 6;
        return std::string(names[band]) + " " + std::to_string(static_cast<int>(std::lround(w / 10.0) * 10)) + " nm";
    };
    // A washed-out film (many orders) is a pale, desaturated grey; a thin film keeps its saturated hue.
    const double keep = std::clamp(0.20 + 0.80 * vis, 0.0, 1.0);
    const double luma = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    const double pale = 0.35 + 0.65 * lightness;
    out.hex = Rgb{(luma + (rgb.r - luma) * keep) * pale, (luma + (rgb.g - luma) * keep) * pale, (luma + (rgb.b - luma) * keep) * pale}.hex();
    for (int w = 400; w <= 700; w += 50) out.lines.push_back({double(w), std::clamp(refl(w), 0.0, 1.0), describe(w)});
    if (out.lines.size() > 4) out.lines[4].name = out.hueNm > 0 ? describe(out.hueNm) : "Neutral grey";   // 600 nm row names the hue
    return out;
}

} // namespace phy104
