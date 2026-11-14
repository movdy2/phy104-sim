#include "ExperimentReports.h"
#include "FieldIds.h"
#include "NumberFormat.h"
#include "PhysicsCalculator.h"
#include "ReportSupport.h"
#include "ThickLens.h"
#include "Uncertainty.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace phy104::ExperimentReports {
namespace NF = NumberFormat;
namespace PC = PhysicsCalculator;
namespace TL = ThickLens;
namespace {

constexpr double kDeg = std::numbers::pi / 180.0;

std::string num(double v) { return NF::general(v, 6); }
std::string fx(double v) { return NF::fixed(v, 4); }

std::string measured(double value, double sigma, const std::string& unit = {}) {
    std::string text = num(value);
    if (sigma > 0.0) text += " ± " + num(sigma);
    if (!unit.empty()) text += " " + unit;
    return text;
}

void validateSigma(std::vector<FieldIssue>& issues, const char* field, double sigma, const char* label) {
    if (!std::isfinite(sigma) || sigma < 0.0)
        issues.push_back({field, std::string("Enter ") + label + " ≥ 0.", label});
}

bool anyUncertainty(std::initializer_list<double> values) {
    return std::any_of(values.begin(), values.end(), [](double value) { return value > 0.0; });
}

std::string ordinal(int m) {
    switch (m) {
    case 0: return "CENTRAL MAXIMUM";
    case 1: return "FIRST BRIGHT FRINGE";
    case 2: return "SECOND BRIGHT FRINGE";
    case 3: return "THIRD BRIGHT FRINGE";
    default: return "BRIGHT FRINGE m = " + std::to_string(m);
    }
}

LcdRegister retainedRegister(std::vector<std::string> lines, const std::string& label, const std::string& value,
                             std::vector<std::string> extra) {
    LcdRegister r;
    r.title = "LAST VALID";
    r.lines = std::move(lines);
    r.resultLabel = label;
    r.resultValue = value;
    r.extraLines = std::move(extra);
    return r;
}

} // namespace

std::string sizeDescription(double magnification) {
    const double a = std::fabs(magnification);
    if (std::fabs(a - 1.0) < 1e-4) return "SAME SIZE (1×)";
    std::string text;
    if (a < 1.0) {
        static const std::pair<double, const char*> fractions[] = {{0.125, "⅛"}, {0.25, "¼"}, {1.0 / 3, "⅓"}, {0.375, "⅜"}, {0.5, "½"},
                                                                    {0.625, "⅝"}, {2.0 / 3, "⅔"}, {0.75, "¾"}, {0.875, "⅞"}};
        for (const auto& [value, glyph] : fractions)
            if (std::fabs(a - value) < 5e-4) { text = glyph; break; }
        if (text.empty()) text = NF::general(a, 3);
        return "DIMINISHED (" + text + "×)";
    }
    return "ENLARGED (" + NF::general(a, 3) + "×)";
}

// ------------------------------------------------------------------------------ refraction & TIR
double defaultTirAngle(double n1, double n2) {
    const auto crit = PC::criticalAngleDeg(n1, n2);
    if (!crit) return 50.0;
    return *crit < 50.0 ? 50.0 : std::min(85.0, *crit + 10.0);
}

namespace {

Report tirReport(const RefractionInput& in, const ReportContext& ctx);

} // namespace

Report refraction(const RefractionInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (!std::isfinite(in.n1) || in.n1 <= 0.0) issues.push_back({FieldIds::refractiveIndex1, "Enter n₁ > 0.", "n₁"});
    if (!std::isfinite(in.n2) || in.n2 <= 0.0) issues.push_back({FieldIds::refractiveIndex2, "Enter n₂ > 0.", "n₂"});
    if (!std::isfinite(in.theta1) || in.theta1 < 0.0 || in.theta1 >= 90.0) issues.push_back({FieldIds::incidentAngle, "Enter 0° ≤ θ₁ < 90°.", "θ₁"});
    validateSigma(issues, FieldIds::sigmaRefractiveIndex1, in.sigmaN1, "σ(n₁)");
    validateSigma(issues, FieldIds::sigmaRefractiveIndex2, in.sigmaN2, "σ(n₂)");
    validateSigma(issues, FieldIds::sigmaIncidentAngle, in.sigmaTheta1, "σ(θ₁)");
    if (!issues.empty()) {
        const std::string first = issues.front().label;
        return ReportSupport::pausedReport("E_INPUT_RANGE", issues,
                            {"n₁ = " + fx(in.n1) + "   n₂ = " + fx(in.n2), "θ₁ = " + fx(in.theta1) + "°", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + first + " out of range",
                             "Refractive indices must be positive and 0° ≤ θ₁ < 90°. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);
    }
    if (in.tirMode) return tirReport(in, ctx);

    const double thetaA = in.theta1;
    const double thetaB = defaultTirAngle(in.n1, in.n2);
    const auto crit = PC::criticalAngleDeg(in.n1, in.n2);
    const std::vector<double> values{in.n1, in.n2, in.theta1};
    const std::vector<double> sigmas{in.sigmaN1, in.sigmaN2, in.sigmaTheta1};
    const UncertainEstimate criticalEstimate = propagateUncertainty({in.n1, in.n2}, {in.sigmaN1, in.sigmaN2},
        [](const std::vector<double>& v) {
            const double ratio = v[1] / v[0];
            return ratio >= 0.0 && ratio < 1.0 ? std::asin(ratio) / kDeg : std::numeric_limits<double>::quiet_NaN();
        });

    Report rep;
    {
        const auto ref = PC::refraction(in.n1, in.n2, thetaA);
        const UncertainEstimate thetaEstimate = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
            const double sin2 = v[0] * std::sin(v[2] * kDeg) / v[1];
            return std::fabs(sin2) <= 1.0 ? std::asin(sin2) / kDeg : std::numeric_limits<double>::quiet_NaN();
        });
        const double sinA = std::sin(thetaA * kDeg);
        const double sin2 = in.n1 * sinA / in.n2;
        LcdRegister r;
        r.lines = {"01  n₁ sin θ₁ = n₂ sin θ₂",
                   "02  sin θ₂ = " + num(in.n1 / in.n2) + " × " + num(std::round(sinA * 1e4) / 1e4),
                   "03  sin θ₂ = " + fx(sin2)};
        r.resultLabel = "REFRACTION ANGLE · θ₂";
        if (ref.totalInternalReflection) {
            r.lines.push_back("04  θ₂ undefined: sin θ₂ > 1");
            r.resultValue = "TIR";
            rep.headline = "θ₂ = TIR";
        } else {
            r.lines.push_back("04  θ₂ = asin(" + fx(sin2) + ")");
            r.resultValue = fx(ref.refractedDeg) + "°";
            rep.headline = "θ₂ = " + fx(ref.refractedDeg) + "°";
            if (thetaEstimate.valid && anyUncertainty({in.sigmaN1, in.sigmaN2, in.sigmaTheta1})) {
                r.resultValue += " ± " + num(thetaEstimate.standardUncertainty) + "°";
                r.extraLines.push_back("σ(θ₂) = " + num(thetaEstimate.standardUncertainty) + "° (1σ)");
                rep.headline += " ± " + num(thetaEstimate.standardUncertainty) + "°";
            }
        }
        rep.registers.push_back(r);
        rep.retained = retainedRegister({"n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "θ₁ = " + NF::fixed(in.theta1, 2) + "°"},
                                        "RETAINED REFRACTION ANGLE · θ₂", r.resultValue, {});
    }
    {
        const double sinB = in.n1 * std::sin(thetaB * kDeg) / in.n2;
        LcdRegister r;
        r.title = "TIR COMPARISON";
        if (crit) {
            r.lines = {"θc = asin(n₂ / n₁)", "θc = " + fx(*crit) + "°",
                       "θ₁ = " + num(thetaB) + "° " + (thetaB > *crit ? ">" : "<") + " θc",
                       "sin θ₂ = " + fx(sinB) + (sinB > 1.0 ? " > 1" : ""),
                       sinB > 1.0 ? "STATUS: TOTAL REFLECTION" : "STATUS: PARTIAL REFRACTION"};
        } else {
            r.lines = {"θc = asin(n₂ / n₁)", "n₁ ≤ n₂: no critical angle", "θ₁ = " + num(thetaB) + "°",
                       "sin θ₂ = " + fx(sinB), "STATUS: REFRACTION ONLY"};
        }
        if (crit && criticalEstimate.valid && anyUncertainty({in.sigmaN1, in.sigmaN2}))
            r.extraLines.push_back("σ(θc) = " + num(criticalEstimate.standardUncertainty) + "° (1σ)");
        rep.registers.push_back(r);
    }
    if (anyUncertainty({in.sigmaN1, in.sigmaN2, in.sigmaTheta1})) {
        const DataSeries sweep = refractionSweep(in);
        std::vector<double> xs, ys, ySigmas;
        for (const auto& row : sweep.rows) if (std::isfinite(row[1])) {
            xs.push_back(row[2]); ys.push_back(row[3]); ySigmas.push_back(row[6]);
        }
        const LinearFit fit = linearFit(xs, ys, ySigmas);
        if (fit.ok) {
            LcdRegister& analysis = rep.registers[1];
            analysis.extraLines.push_back(std::string(fit.weighted ? "WEIGHTED FIT · " : "OLS FIT · ") +
                "slope = " + num(fit.slope) + " ± " + num(fit.slopeUncertainty) + " (1σ)");
            const std::string fitQuality = fit.degreesOfFreedom <= 0 ? "fit quality = n/a" :
                fit.weighted ? "χ²/ν = " + num(fit.chiSquared / fit.degreesOfFreedom) :
                               "residual s = " + num(std::sqrt(fit.chiSquared / fit.degreesOfFreedom));
            analysis.extraLines.push_back("intercept = " + num(fit.intercept) + " ± " + num(fit.interceptUncertainty) + " (1σ)"+
                " · R² = " + num(fit.r2) + " · " + fitQuality);
        }
    }
    rep.notices.push_back({"Physical limit, not a solver error",
                           "For θ₁ > θc, no real refracted angle exists. TIR is reported instead of evaluating asin outside [−1, 1]. "
                           "Entered uncertainties are independent 1σ standard uncertainties.", false});
    rep.parameters = {{"Mode", "Refraction"}, {"n₁", measured(in.n1, in.sigmaN1)}, {"n₂", measured(in.n2, in.sigmaN2)},
                      {"θ₁ (incidence)", measured(in.theta1, in.sigmaTheta1, "°")}};
    if (crit && criticalEstimate.valid && anyUncertainty({in.sigmaN1, in.sigmaN2}))
        rep.parameters.emplace_back("Critical angle θc", fx(*crit) + "° ± " + num(criticalEstimate.standardUncertainty) + "°");
    return rep;
}

namespace {

/// The single-case TIR screen of the design.
Report tirReport(const RefractionInput& in, const ReportContext&) {
    const auto crit = PC::criticalAngleDeg(in.n1, in.n2);
    const double th = in.theta1;
    const double sin2 = in.n1 * std::sin(th * kDeg) / in.n2;
    const bool tir = crit && th > *crit && sin2 > 1.0;
    const std::vector<double> values{in.n1, in.n2, th};
    const std::vector<double> sigmas{in.sigmaN1, in.sigmaN2, in.sigmaTheta1};
    const UncertainEstimate criticalEstimate = propagateUncertainty({in.n1, in.n2}, {in.sigmaN1, in.sigmaN2},
        [](const std::vector<double>& v) {
            const double ratio = v[1] / v[0];
            return ratio >= 0.0 && ratio < 1.0 ? std::asin(ratio) / kDeg : std::numeric_limits<double>::quiet_NaN();
        });

    Report rep;
    LcdRegister r1;
    r1.title = "CRITICAL ANGLE REGISTER";
    r1.mode = "OK";
    r1.lines.push_back("01 θc = asin(n₂ / n₁)");
    if (crit) {
        r1.lines.push_back("02 θc = asin(" + NF::fixed(in.n2, 1) + " / " + NF::fixed(in.n1, 1) + ")");
        r1.lines.push_back("03 θc = " + fx(*crit) + "°");
        r1.lines.push_back("04 θ₁ = " + fx(th) + "° " + (th > *crit ? ">" : "<") + " θc");
        r1.lines.push_back("05 " + num(in.n1) + " sin(" + num(th) + "°) = " + fx(sin2));
        r1.lines.push_back(tir ? "06 " + fx(sin2) + " > 1 → no real θ₂" : "06 " + fx(sin2) + " ≤ 1 → real θ₂ exists");
    } else {
        r1.lines.push_back("02 n₁ ≤ n₂: sin θc would exceed 1");
        r1.lines.push_back("03 θc = undefined");
        r1.lines.push_back("04 light enters a denser medium");
        r1.lines.push_back("05 " + num(in.n1) + " sin(" + num(th) + "°) = " + fx(sin2));
        r1.lines.push_back("06 " + fx(sin2) + " ≤ 1 → refraction only");
    }
    if (crit && criticalEstimate.valid && anyUncertainty({in.sigmaN1, in.sigmaN2}))
        r1.extraLines.push_back("σ(θc) = " + num(criticalEstimate.standardUncertainty) + "° (1σ)");
    r1.resultLabel = "PHYSICAL OUTCOME";
    r1.resultValue = tir ? "TIR" : (crit ? "PARTIAL" : "NO TIR");
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = "REFLECTION REGISTER";
    r2.mode = "OK";
    r2.lines.push_back("θᵣ = θ₁ = " + fx(th) + "°");
    if (in.sigmaTheta1 > 0.0) r2.extraLines.push_back("σ(θᵣ) = " + num(in.sigmaTheta1) + "° (1σ)");
    if (tir) {
        r2.lines.push_back("TRANSMISSION: NONE");
        r2.lines.push_back("STATUS: VALID / TIR");
        r2.resultLabel = "ABOVE CRITICAL BY";
        r2.resultValue = fx(th - *crit) + "°";
    } else {
        r2.lines.push_back("TRANSMISSION: θ₂ = " + fx(std::asin(std::min(1.0, sin2)) / kDeg) + "°");
        r2.lines.push_back(std::string("STATUS: VALID / ") + (crit ? "REFRACTION" : "NO TIR"));
        if (crit) { r2.resultLabel = "BELOW CRITICAL BY"; r2.resultValue = fx(*crit - th) + "°"; }
        if (sin2 <= 1.0 && anyUncertainty({in.sigmaN1, in.sigmaN2, in.sigmaTheta1})) {
            const UncertainEstimate thetaEstimate = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
                const double q = v[0] * std::sin(v[2] * kDeg) / v[1];
                return std::fabs(q) <= 1.0 ? std::asin(q) / kDeg : std::numeric_limits<double>::quiet_NaN();
            });
            if (thetaEstimate.valid) r2.extraLines.push_back("σ(θ₂) = " + num(thetaEstimate.standardUncertainty) + "° (1σ)");
        }
    }
    if (crit && anyUncertainty({in.sigmaN1, in.sigmaN2, in.sigmaTheta1})) {
        const double sign = tir ? 1.0 : -1.0;
        const UncertainEstimate margin = propagateUncertainty(values, sigmas, [sign](const std::vector<double>& v) {
            const double ratio = v[1] / v[0];
            if (ratio < 0.0 || ratio >= 1.0) return std::numeric_limits<double>::quiet_NaN();
            const double critical = std::asin(ratio) / kDeg;
            return sign * (v[2] - critical);
        });
        if (margin.valid) r2.resultValue += " ± " + num(margin.standardUncertainty) + "°";
    }
    rep.registers.push_back(r2);

    const std::string n1 = in.name1, n2 = in.name2;
    if (tir)
        rep.notices.push_back({"Valid physical outcome, not an error",
                               "All physical rays remain in " + ReportSupport::lowercaseAscii(n1) + ". Above θc there is no propagating transmitted ray; the sweep stops at θ₂ = 90° at the critical limit.", false});
    else
        rep.notices.push_back({"Refraction occurs, no TIR",
                               crit ? "θ₁ is below θc = " + fx(*crit) + "°, so part of the light is transmitted into " + ReportSupport::lowercaseAscii(n2) + " and part is reflected."
                                    : "TIR requires n₁ > n₂. Going into a denser medium the ray always refracts.", false});
    rep.headline = tir ? "θc = " + fx(*crit) + "° · TIR" : "θ₂ = " + fx(std::asin(std::min(1.0, sin2)) / kDeg) + "°";
    if (tir && crit && criticalEstimate.valid && anyUncertainty({in.sigmaN1, in.sigmaN2}))
        rep.headline = "θc = " + fx(*crit) + "° ± " + num(criticalEstimate.standardUncertainty) + "° · TIR";
    if (!tir && anyUncertainty({in.sigmaN1, in.sigmaN2, in.sigmaTheta1})) {
        const UncertainEstimate thetaEstimate = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
            const double q = v[0] * std::sin(v[2] * kDeg) / v[1];
            return std::fabs(q) <= 1.0 ? std::asin(q) / kDeg : std::numeric_limits<double>::quiet_NaN();
        });
        if (thetaEstimate.valid) rep.headline += " ± " + num(thetaEstimate.standardUncertainty) + "°";
    }
    rep.statusText = tir ? "Valid outcome: TIR" : "Valid outcome: refraction";
    rep.retained = retainedRegister({"n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "θ₁ = " + NF::fixed(th, 2) + "°"},
                                    "RETAINED OUTCOME", r1.resultValue, {});
    rep.parameters = {{"Mode", "TIR"}, {"n₁", measured(in.n1, in.sigmaN1)}, {"n₂", measured(in.n2, in.sigmaN2)},
                      {"θ₁ (incidence)", measured(th, in.sigmaTheta1, "°")}};
    return rep;
}

} // namespace

DataSeries refractionSweep(const RefractionInput& in) {
    DataSeries s{"refraction_sweep", "Refraction sweep",
                 in.name1 + " → " + in.name2 + " · n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2),
                 "sin θ is dimensionless; uncertainty columns are independent 1σ standard uncertainties",
                 {"θ₁ (°)", "θ₂ (°)", "sin θ₁", "sin θ₂", "σ θ₂ (°)", "σ sin θ₁", "σ sin θ₂"}, {}, {}};
    for (int a = 0; a <= 35; a += 5) {
        const std::vector<double> values{in.n1, in.n2, double(a)};
        const std::vector<double> sigmas{in.sigmaN1, in.sigmaN2, in.sigmaTheta1};
        const double sin1 = std::sin(a * kDeg), sin2 = in.n1 * sin1 / in.n2;
        const double th2 = std::fabs(sin2) > 1.0 ? kNaN : std::asin(sin2) / kDeg;
        const auto thetaError = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
            const double z = v[0] * std::sin(v[2] * kDeg) / v[1];
            return std::fabs(z) <= 1.0 ? std::asin(z) / kDeg : kNaN;
        });
        const auto sin1Error = propagateUncertainty({double(a)}, {in.sigmaTheta1}, [](const std::vector<double>& v) {
            return std::sin(v[0] * kDeg);
        });
        const auto sin2Error = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
            return v[0] * std::sin(v[2] * kDeg) / v[1];
        });
        s.rows.push_back({double(a), th2, sin1, sin2,
                          thetaError.valid ? thetaError.standardUncertainty : kNaN,
                          sin1Error.valid ? sin1Error.standardUncertainty : kNaN,
                          sin2Error.valid ? sin2Error.standardUncertainty : kNaN});
    }
    s.analysis = {{"Input n₁", measured(in.n1, in.sigmaN1)}, {"Input n₂", measured(in.n2, in.sigmaN2)},
                  {"Input σ(θ₁)", num(in.sigmaTheta1) + "° (1σ)"}};
    std::vector<double> xs, ys, ySigmas;
    for (const auto& row : s.rows) if (std::isfinite(row[3])) {
        xs.push_back(row[2]); ys.push_back(row[3]); ySigmas.push_back(row[6]);
    }
    const LinearFit fit = linearFit(xs, ys, ySigmas);
    if (fit.ok) {
        s.analysis.emplace_back("Fit", fit.weighted ? "Weighted least squares (y uncertainties)" : "Ordinary least squares");
        s.analysis.emplace_back("Slope", num(fit.slope));
        s.analysis.emplace_back("Slope standard uncertainty (1σ)", num(fit.slopeUncertainty));
        s.analysis.emplace_back("Intercept", num(fit.intercept));
        s.analysis.emplace_back("Intercept standard uncertainty (1σ)", num(fit.interceptUncertainty));
        s.analysis.emplace_back("R²", num(fit.r2));
        if (fit.weighted) {
            s.analysis.emplace_back("χ²", num(fit.chiSquared));
            if (fit.degreesOfFreedom > 0)
                s.analysis.emplace_back("Reduced χ²", num(fit.chiSquared / fit.degreesOfFreedom));
        } else {
            s.analysis.emplace_back("Residual sum of squares", num(fit.chiSquared));
            if (fit.degreesOfFreedom > 0)
                s.analysis.emplace_back("Residual standard error", num(std::sqrt(fit.chiSquared / fit.degreesOfFreedom)));
        }
        s.analysis.emplace_back("Degrees of freedom", std::to_string(fit.degreesOfFreedom));
    }
    return s;
}

DataSeries tirSweep(const RefractionInput& in) {
    DataSeries s{"critical_sweep", "Critical-angle sweep",
                 in.name1 + " → " + in.name2 + " · n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2),
                 "θ₂ is empty where TIR occurs; σ columns are 1σ standard uncertainties",
                 {"θ₁ (°)", "θ₂ (°)", "σ θ₂ (°)"}, {}, {}};
    s.analysis = {{"Input n₁", measured(in.n1, in.sigmaN1)}, {"Input n₂", measured(in.n2, in.sigmaN2)},
                  {"Input σ(θ₁)", num(in.sigmaTheta1) + "° (1σ)"}};
    for (int a = 0; a <= 90; a += 5) {
        const std::vector<double> values{in.n1, in.n2, double(a)};
        const std::vector<double> sigmas{in.sigmaN1, in.sigmaN2, in.sigmaTheta1};
        const double sin2 = in.n1 * std::sin(a * kDeg) / in.n2;
        const double theta2 = std::fabs(sin2) > 1.0 ? kNaN : std::asin(sin2) / kDeg;
        const auto sigma = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
            const double z = v[0] * std::sin(v[2] * kDeg) / v[1];
            return std::fabs(z) <= 1.0 ? std::asin(z) / kDeg : kNaN;
        });
        s.rows.push_back({double(a), theta2, sigma.valid ? sigma.standardUncertainty : kNaN});
    }
    return s;
}

LinearFit linearFit(const std::vector<double>& x, const std::vector<double>& y,
                    const std::vector<double>& yStandardUncertainties) {
    LinearFit f;
    const std::size_t n = x.size();
    if (n < 2 || y.size() != n) return f;
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isfinite(x[i]) || !std::isfinite(y[i])) return f;

    if (yStandardUncertainties.size() == n) {
        double largestSigma = 0.0;
        bool allValid = true, anyPositive = false;
        for (double sigma : yStandardUncertainties) {
            if (!std::isfinite(sigma) || sigma < 0.0) { allValid = false; break; }
            largestSigma = std::max(largestSigma, sigma);
            anyPositive = anyPositive || sigma > 0.0;
        }
        if (allValid && anyPositive) {
            std::vector<double> positiveSigmas = yStandardUncertainties;
            const double floor = largestSigma * 1e-12;
            for (double& sigma : positiveSigmas) if (sigma == 0.0) sigma = floor;
            const WeightedLinearFit weighted = weightedLinearFit(x, y, positiveSigmas);
            if (weighted.ok) {
                f.slope = weighted.slope;
                f.intercept = weighted.intercept;
                f.r2 = weighted.r2;
                f.slopeUncertainty = weighted.slopeUncertainty;
                f.interceptUncertainty = weighted.interceptUncertainty;
                f.chiSquared = weighted.chiSquared;
                f.degreesOfFreedom = weighted.degreesOfFreedom;
                f.weighted = true;
                f.ok = true;
                return f;
            }
        }
    }

    const double count = static_cast<double>(n);
    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; syy += y[i] * y[i];
    }
    const double den = count * sxx - sx * sx;
    if (!std::isfinite(den) || den <= std::numeric_limits<double>::epsilon() * count * sxx) return f;
    f.slope = (count * sxy - sx * sy) / den;
    f.intercept = (sy - f.slope * sx) / count;
    const double ssTot = std::max(0.0, syy - sy * sy / count);
    double ssRes = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double residual = y[i] - (f.slope * x[i] + f.intercept);
        ssRes += residual * residual;
    }
    f.r2 = ssTot < 1e-300 ? 1.0 : 1.0 - ssRes / ssTot;
    f.chiSquared = ssRes;
    f.degreesOfFreedom = static_cast<int>(n) - 2;
    if (f.degreesOfFreedom > 0) {
        const double residualVariance = ssRes / f.degreesOfFreedom;
        f.slopeUncertainty = std::sqrt(residualVariance * count / den);
        f.interceptUncertainty = std::sqrt(residualVariance * sxx / den);
    }
    f.ok = std::isfinite(f.slope) && std::isfinite(f.intercept) && std::isfinite(f.r2) &&
           std::isfinite(f.slopeUncertainty) && std::isfinite(f.interceptUncertainty) && std::isfinite(f.chiSquared);
    return f;
}

// ------------------------------------------------------------------------------ lenses & mirrors
Report imaging(const ImagingInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (!std::isfinite(in.f) || in.f == 0.0) issues.push_back({FieldIds::focalLength, "Enter f ≠ 0.", "f"});
    if (!std::isfinite(in.s) || in.s <= 0.0) issues.push_back({FieldIds::objectDistance, "Enter dₒ > 0.", "dₒ"});
    if (!std::isfinite(in.h) || in.h <= 0.0) issues.push_back({FieldIds::objectHeight, "Enter hₒ > 0.", "hₒ"});
    validateSigma(issues, FieldIds::sigmaFocalLength, in.sigmaF, "σ(f)");
    validateSigma(issues, FieldIds::sigmaObjectDistance, in.sigmaS, "σ(dₒ)");
    validateSigma(issues, FieldIds::sigmaObjectHeight, in.sigmaH, "σ(hₒ)");
    if (!issues.empty())
        return ReportSupport::pausedReport("E_INPUT_RANGE", issues,
                            {"f = " + num(in.f) + " cm   dₒ = " + num(in.s) + " cm", "hₒ = " + num(in.h) + " cm", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + issues.front().label + " out of range",
                             "Enter a non-zero focal length and positive object distance and height. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const auto img = PC::imageFormation(in.s, in.f);
    if (img.atInfinity)
        return ReportSupport::pausedReport("E_FOCAL_PLANE", {{FieldIds::objectDistance, "Object in the focal plane: move dₒ away from f.", "dₒ"}},
                            {"dₒ = f = " + num(in.s) + " cm", "dᵢ → ∞  (image at infinity)", "m → undefined", "ACTION: MOVE dₒ AWAY FROM f"},
                            {"Guarded case / E_FOCAL_PLANE",
                             "Comparison: dₒ = f = " + num(in.s) + " cm. Image at infinity; finite dᵢ and m suppressed. Move the object away from the focal plane to calculate a finite image.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const double m = img.magnification;
    const std::vector<double> values{in.f, in.s, in.h};
    const std::vector<double> sigmas{in.sigmaF, in.sigmaS, in.sigmaH};
    const auto imageDistanceEstimate = propagateUncertainty({in.f, in.s}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
        const double denominator = 1.0 / v[0] - 1.0 / v[1];
        return std::fabs(denominator) > 1e-15 ? 1.0 / denominator : std::numeric_limits<double>::quiet_NaN();
    });
    const auto magnificationEstimate = propagateUncertainty({in.f, in.s}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
        const double denominator = 1.0 / v[0] - 1.0 / v[1];
        return std::fabs(denominator) > 1e-15 ? -1.0 / (denominator * v[1]) : std::numeric_limits<double>::quiet_NaN();
    });
    const auto heightEstimate = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
        const double denominator = 1.0 / v[0] - 1.0 / v[1];
        return std::fabs(denominator) > 1e-15 ? -v[2] / (denominator * v[1]) : std::numeric_limits<double>::quiet_NaN();
    });
    const bool uncertain = anyUncertainty({in.sigmaF, in.sigmaS, in.sigmaH});
    Report rep;
    LcdRegister r1;
    const std::string fText = in.f < 0 ? "(" + num(in.f) + ")" : num(in.f);
    r1.lines = {"01  1/f = 1/dₒ + 1/dᵢ", "02  1/dᵢ = 1/" + fText + " - 1/" + num(in.s)};
    const std::string invDi = NF::fixed(1.0 / in.f - 1.0 / in.s, 6);
    if (in.mirrorMode) {
        r1.title = "GAUSSIAN MIRROR REGISTER";
        r1.mode = "OK";
        r1.lines.push_back("03  1/dᵢ = " + invDi + " cm⁻¹");
        r1.lines.push_back("04  dᵢ = (" + invDi + ")⁻¹");
        r1.resultLabel = "IMAGE DISTANCE / cm";
        r1.resultValue = fx(img.imageDistance);
    } else {
        r1.lines.push_back("03  1/dᵢ = " + invDi + " cm⁻¹");
        r1.lines.push_back("04  dᵢ = " + NF::signedFixed(img.imageDistance) + " cm");
        r1.resultLabel = "MAGNIFICATION · m = -dᵢ/dₒ";
        r1.resultValue = fx(m);
    }
    if (uncertain) {
        if (imageDistanceEstimate.valid) r1.extraLines.push_back("σ(dᵢ) = " + num(imageDistanceEstimate.standardUncertainty) + " cm (1σ)");
        if (magnificationEstimate.valid) r1.extraLines.push_back("σ(m) = " + num(magnificationEstimate.standardUncertainty) + " (1σ)");
        if (in.mirrorMode && imageDistanceEstimate.valid)
            r1.resultValue += " ± " + num(imageDistanceEstimate.standardUncertainty) + " cm";
        else if (magnificationEstimate.valid)
            r1.resultValue += " ± " + num(magnificationEstimate.standardUncertainty);
    }
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = "IMAGE PROPERTIES";
    if (in.mirrorMode) {
        r2.mode = "OK";
        r2.lines = {"m = -dᵢ/dₒ = " + NF::signedFixed(m), "hᵢ = m × hₒ = " + NF::signedFixed(m * in.h) + " cm",
                    std::string("IMAGE: ") + (img.real ? "REAL" : "VIRTUAL"), std::string("ORIENTATION: ") + (img.upright ? "UPRIGHT" : "INVERTED"),
                    "SIZE: " + sizeDescription(m)};
    } else {
        r2.lines = {"hᵢ = m × hₒ", "hᵢ = " + fx(m * in.h) + " cm", std::string("IMAGE: ") + (img.real ? "REAL" : "VIRTUAL"),
                    std::string("ORIENTATION: ") + (img.upright ? "UPRIGHT" : "INVERTED"), "MIRROR: SAME RESULTS"};
    }
    if (uncertain && heightEstimate.valid)
        r2.extraLines.push_back("σ(hᵢ) = " + num(heightEstimate.standardUncertainty) + " cm (1σ)");
    rep.registers.push_back(r2);

    if (in.mirrorMode) {
        rep.notices.push_back(img.imageDistance < 0
            ? Notice{"Negative distance ≠ invalid input",
                     "dᵢ < 0 locates the virtual image behind the mirror" + std::string(in.f < 0 ? ", between P and F" : "") +
                     ". Positive magnification means upright; |m| < 1 means diminished.", false}
            : Notice{"Real image in front of the mirror",
                     "dᵢ > 0 locates the real image in front of the mirror, on the same side as the object. Negative magnification means inverted.", false});
        rep.footnote = "P: pole · F: " + NF::general(in.f, 5) + " cm · C: " + NF::general(2 * in.f, 5) + " cm";
    } else {
        rep.notices.push_back({"Guarded case / E_FOCAL_PLANE",
                               "Comparison: dₒ = f = " + num(in.f) + " cm. Image at infinity; finite dᵢ and m suppressed. Move the object away from the focal plane to calculate a finite image.", true});
    }
    rep.headline = "dᵢ = " + NF::signedFixed(img.imageDistance) + " cm";
    if (uncertain && imageDistanceEstimate.valid)
        rep.headline += " ± " + num(imageDistanceEstimate.standardUncertainty) + " cm";
    rep.statusText = img.real ? "Real image · positive distances" : "Virtual image · negative distances are valid";
    std::string retainedDi = NF::signedFixed(img.imageDistance) + " cm";
    std::vector<std::string> retainedExtra{"m = " + fx(m) + " · hᵢ = " + fx(m * in.h) + " cm"};
    if (uncertain && imageDistanceEstimate.valid) retainedDi += " ± " + num(imageDistanceEstimate.standardUncertainty) + " cm";
    if (uncertain && magnificationEstimate.valid)
        retainedExtra.push_back("σ(m) = " + num(magnificationEstimate.standardUncertainty) + " (1σ)");
    if (uncertain && heightEstimate.valid)
        retainedExtra.push_back("σ(hᵢ) = " + num(heightEstimate.standardUncertainty) + " cm (1σ)");
    rep.retained = retainedRegister({"f = " + NF::signedFixed(in.f, 2) + " cm · dₒ = " + NF::fixed(in.s, 2) + " cm", "hₒ = " + NF::fixed(in.h, 2) + " cm"},
                                    "RETAINED IMAGE DISTANCE · dᵢ", retainedDi, std::move(retainedExtra));
    rep.parameters = {{"Element", std::string(in.mirrorMode ? "Mirror" : "Thin lens") + (in.f < 0 ? " (diverging / convex)" : " (converging / concave)")},
                      {"f", measured(in.f, in.sigmaF, "cm")}, {"dₒ", measured(in.s, in.sigmaS, "cm")},
                      {"hₒ", measured(in.h, in.sigmaH, "cm")}};
    return rep;
}

DataSeries imagingSweep(const ImagingInput& in) {
    char desc[128];
    std::snprintf(desc, sizeof desc, "f = %s cm · hₒ = %s cm", NF::general(in.f, 5).c_str(), NF::general(in.h, 5).c_str());
    DataSeries s{"imaging_sweep", in.mirrorMode ? "Spherical mirror sweep" : "Thin lens sweep", desc,
                 "negative dᵢ = virtual image; σ columns are independent 1σ uncertainties",
                 {"dₒ (cm)", "dᵢ (cm)", "m", "σ dᵢ (cm)", "σ m"}, {}, {}};
    s.analysis = {{"Input focal length f", measured(in.f, in.sigmaF, "cm")},
                  {"Input object distance dₒ", measured(in.s, in.sigmaS, "cm")},
                  {"Input object height hₒ", measured(in.h, in.sigmaH, "cm")}};
    const double af = std::fabs(in.f);
    for (int k = 0; k < 8; ++k) {
        const double so = af * (1.25 + 0.5 * k);
        const auto img = PC::imageFormation(so, in.f);
        const auto diError = propagateUncertainty({in.f, so}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
            const double denominator = 1.0 / v[0] - 1.0 / v[1];
            return std::fabs(denominator) > 1e-15 ? 1.0 / denominator : kNaN;
        });
        const auto mError = propagateUncertainty({in.f, so}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
            const double denominator = 1.0 / v[0] - 1.0 / v[1];
            return std::fabs(denominator) > 1e-15 ? -1.0 / (denominator * v[1]) : kNaN;
        });
        s.rows.push_back({so, img.atInfinity ? kNaN : img.imageDistance, img.atInfinity ? kNaN : img.magnification,
                          img.atInfinity || !diError.valid ? kNaN : diError.standardUncertainty,
                          img.atInfinity || !mError.valid ? kNaN : mError.standardUncertainty});
    }
    const auto current = PC::imageFormation(in.s, in.f);
    if (!current.atInfinity) {
        const auto diError = propagateUncertainty({in.f, in.s}, {in.sigmaF, in.sigmaS}, [](const std::vector<double>& v) {
            const double denominator = 1.0 / v[0] - 1.0 / v[1];
            return std::fabs(denominator) > 1e-15 ? 1.0 / denominator : kNaN;
        });
        if (diError.valid) s.analysis.emplace_back("Current image distance dᵢ", measured(current.imageDistance, diError.standardUncertainty, "cm"));
    }
    return s;
}

// ------------------------------------------------------------------------------ prisms
Report prism(const PrismInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (!std::isfinite(in.apex) || in.apex <= 0.0 || in.apex >= 180.0) issues.push_back({FieldIds::prismApex, "Enter 0° < A < 180°.", "A"});
    if (!std::isfinite(in.incidence) || in.incidence < 0.0 || in.incidence >= 90.0) issues.push_back({FieldIds::incidenceAngle, "Enter 0° ≤ i₁ < 90°.", "i₁"});
    if (!std::isfinite(in.wavelengthNm) || in.wavelengthNm <= 0.0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (!std::isfinite(in.nPrism) || in.nPrism <= 0.0) issues.push_back({FieldIds::prismIndex, "Enter n prism > 0.", "n prism"});
    if (!std::isfinite(in.nMedium) || in.nMedium <= 0.0) issues.push_back({FieldIds::surroundingIndex, "Enter n medium > 0.", "n medium"});
    validateSigma(issues, FieldIds::sigmaPrismIndex, in.sigmaNPrism, "σ(n prism)");
    validateSigma(issues, FieldIds::sigmaMediumIndex, in.sigmaNMedium, "σ(n medium)");
    validateSigma(issues, FieldIds::sigmaPrismApex, in.sigmaApex, "σ(A)");
    validateSigma(issues, FieldIds::sigmaPrismIncidence, in.sigmaIncidence, "σ(i₁)");
    validateSigma(issues, FieldIds::sigmaWavelength, in.sigmaWavelengthNm, "σ(λ)");
    if (!issues.empty())
        return ReportSupport::pausedReport("E_INPUT_RANGE", issues,
                            {"A = " + num(in.apex) + "°   i₁ = " + num(in.incidence) + "°", "λ = " + num(in.wavelengthNm) + " nm", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + issues.front().label + " out of range",
                             "Apex 0° < A < 180°, incidence 0° ≤ i₁ < 90°, wavelength > 0. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const double n = in.nPrism / in.nMedium;
    const double sigmaPrismIndex = std::hypot(in.sigmaNPrism, in.dnPrismDlambda * in.sigmaWavelengthNm);
    const double sigmaMediumIndex = std::hypot(in.sigmaNMedium, in.dnMediumDlambda * in.sigmaWavelengthNm);
    const std::vector<double> prismValues{in.nPrism, in.nMedium, in.apex, in.incidence, in.wavelengthNm};
    const std::vector<double> prismSigmas{in.sigmaNPrism, in.sigmaNMedium, in.sigmaApex, in.sigmaIncidence, in.sigmaWavelengthNm};
    const bool uncertain = anyUncertainty({sigmaPrismIndex, sigmaMediumIndex, in.sigmaApex, in.sigmaIncidence});
    const auto deviationEstimate = propagateUncertainty(prismValues, prismSigmas, [&in](const std::vector<double>& v) {
        const double deltaLambda = v[4] - in.wavelengthNm;
        const double nPrism = v[0] + in.dnPrismDlambda * deltaLambda;
        const double nMedium = v[1] + in.dnMediumDlambda * deltaLambda;
        const auto result = PC::prism(nPrism, nMedium, v[2], v[3]);
        return result.valid && !result.entryTir && !result.exitTir ? result.deviation : kNaN;
    });
    const auto ratioEstimate = propagateUncertainty({in.nPrism, in.nMedium, in.wavelengthNm},
        {in.sigmaNPrism, in.sigmaNMedium, in.sigmaWavelengthNm}, [&in](const std::vector<double>& v) {
            const double deltaLambda = v[2] - in.wavelengthNm;
            const double nPrism = v[0] + in.dnPrismDlambda * deltaLambda;
            const double nMedium = v[1] + in.dnMediumDlambda * deltaLambda;
            return nPrism / nMedium;
        });
    const auto p = PC::prism(in.nPrism, in.nMedium, in.apex, in.incidence);
    const auto dmin = PC::minimumDeviationDeg(in.nPrism, in.nMedium, in.apex);

    Report rep;
    if (p.exitTir || p.entryTir) {
        LcdRegister r;
        r.lines = {"r₁ = " + fx(p.r1) + "°", "r₂ = A − r₁ = " + fx(p.r2) + "°", "n sin r₂ = " + fx(n * std::sin(p.r2 * kDeg)) + " > 1"};
        r.resultLabel = "PHYSICAL OUTCOME";
        r.resultValue = "TIR";
        rep.registers.push_back(r);
        rep.valid = true;                                          // a valid physical outcome, not an input error
        rep.errorCode = "E_EXIT_TIR";
        rep.notices.push_back({"Exit-face validation", "n sin r₂ > 1: the exit ray undergoes TIR. The solver reports no emergent ray rather than an invalid angle.", false});
        rep.headline = "δ = no emergent ray";
        rep.statusText = "Valid outcome: TIR at the exit face";
        rep.retained = retainedRegister({"n = " + fx(n) + " · A = " + NF::fixed(in.apex, 2) + "°", "i₁ = " + fx(in.incidence) + "°"}, "RETAINED OUTCOME", "TIR", {});
        rep.parameters = {{"Mode", in.dispersion ? "Dispersion" : "Single ray"},
                          {"n (prism / medium)", measured(n, ratioEstimate.valid ? ratioEstimate.standardUncertainty : 0.0)},
                          {"n prism", measured(in.nPrism, sigmaPrismIndex)}, {"n medium", measured(in.nMedium, sigmaMediumIndex)},
                          {"Apex A", measured(in.apex, in.sigmaApex, "°")},
                          {"Incidence i₁", measured(in.incidence, in.sigmaIncidence, "°")},
                          {"Wavelength λ", measured(in.wavelengthNm, in.sigmaWavelengthNm, "nm")}};
        if (uncertain) rep.parameters.emplace_back("Uncertainty convention", "Independent 1σ inputs; wavelength affects n through the preset dispersion model");
        return rep;
    }

    const bool symmetric = std::fabs(p.i1 - p.i2) < 0.005;
    LcdRegister r;
    if (in.dispersion) r.lines.push_back("λ = " + NF::general(in.wavelengthNm, 5) + " nm   n = " + fx(n));
    r.lines.push_back("01  r₁ = asin(sin i₁ / n)");
    r.lines.push_back("    r₁ = " + fx(p.r1) + "°");
    r.lines.push_back("02  r₂ = A - r₁ = " + num(std::round(p.r2 * 1e4) / 1e4) + "°");
    r.lines.push_back("03  i₂ = asin(n sin r₂)");
    r.lines.push_back("    i₂ = " + fx(p.i2) + "°");
    r.lines.push_back("04  δ = i₁ + i₂ - A");
    r.resultLabel = symmetric ? "MINIMUM DEVIATION · δmin" : "DEVIATION · δ";
    r.resultValue = fx(p.deviation) + "°";
    if (uncertain && deviationEstimate.valid) {
        r.resultValue += " ± " + num(deviationEstimate.standardUncertainty) + "°";
        r.extraLines.push_back("σ(δ) = " + num(deviationEstimate.standardUncertainty) + "° (1σ)");
    }
    rep.registers.push_back(r);

    if (symmetric)
        rep.notices.push_back({"Symmetry verified", "i₁ = i₂ and r₁ = r₂ = A/2. The ray is at the stationary point of the deviation curve.", false});
    else if (dmin)
        rep.notices.push_back({"Off the minimum", "i₁ ≠ i₂. δmin = " + fx(*dmin) + "° occurs at i₁ = " + fx((in.apex + *dmin) / 2.0) + "° (symmetric passage).", false});
    rep.notices.push_back({"Exit-face validation", "If n sin r₂ > 1, the exit ray undergoes TIR. The solver reports no emergent ray rather than an invalid angle.", false});

    if (dmin) {
        const auto nBack = PC::indexFromMinimumDeviation(*dmin, in.apex);
        rep.footnote = "n = sin[(A + δmin)/2]\n    / sin(A/2)\n  = " + fx(nBack.value_or(n));
    }
    rep.headline = std::string(symmetric ? "δmin" : "δ") + " = " + fx(p.deviation) + "°";
    if (uncertain && deviationEstimate.valid) rep.headline += " ± " + num(deviationEstimate.standardUncertainty) + "°";
    std::vector<std::string> retainedExtra{"i₂ = " + fx(p.i2) + "°"};
    if (uncertain && deviationEstimate.valid)
        retainedExtra.push_back("σ(δ) = " + num(deviationEstimate.standardUncertainty) + "° (1σ)");
    rep.retained = retainedRegister({"n = " + fx(n) + " · A = " + NF::fixed(in.apex, 2) + "°", "i₁ = " + fx(in.incidence) + "°"},
                                    "RETAINED DEVIATION · δ", r.resultValue, std::move(retainedExtra));
    rep.parameters = {{"Mode", in.dispersion ? "Dispersion" : "Single ray"},
                      {"n (prism / medium)", measured(n, ratioEstimate.valid ? ratioEstimate.standardUncertainty : 0.0)},
                      {"n prism", measured(in.nPrism, sigmaPrismIndex)}, {"n medium", measured(in.nMedium, sigmaMediumIndex)},
                      {"Apex A", measured(in.apex, in.sigmaApex, "°")},
                      {"Incidence i₁", measured(in.incidence, in.sigmaIncidence, "°")},
                      {"Wavelength λ", measured(in.wavelengthNm, in.sigmaWavelengthNm, "nm")}};
    if (uncertain) rep.parameters.emplace_back("Uncertainty convention", "Independent 1σ inputs; wavelength affects n through the preset dispersion model");
    return rep;
}

DataSeries prismSweep(const PrismInput& in) {
    const double sigmaPrismIndex = std::hypot(in.sigmaNPrism, in.dnPrismDlambda * in.sigmaWavelengthNm);
    const double sigmaMediumIndex = std::hypot(in.sigmaNMedium, in.dnMediumDlambda * in.sigmaWavelengthNm);
    DataSeries s{"prism_sweep", "Prism deviation sweep", "n = " + fx(in.nPrism / in.nMedium) + " · A = " + NF::general(in.apex, 5) + "°",
                 "σ columns are independent 1σ standard uncertainties",
                 {"i₁ (°)", "δ (°)", "σ i₁ (°)", "σ δ (°)"}, {}, {}};
    s.analysis = {{"Input n prism", measured(in.nPrism, in.sigmaNPrism)},
                  {"Input n medium", measured(in.nMedium, in.sigmaNMedium)},
                  {"Effective σ(n prism)", num(sigmaPrismIndex)}, {"Effective σ(n medium)", num(sigmaMediumIndex)},
                  {"Input apex A", measured(in.apex, in.sigmaApex, "°")},
                  {"Input incidence i₁", measured(in.incidence, in.sigmaIncidence, "°")},
                  {"Input wavelength λ", measured(in.wavelengthNm, in.sigmaWavelengthNm, "nm")}};
    for (int a = 35; a <= 65; a += 5) {
        const std::vector<double> values{in.nPrism, in.nMedium, in.apex, double(a), in.wavelengthNm};
        const std::vector<double> sigmas{in.sigmaNPrism, in.sigmaNMedium, in.sigmaApex, in.sigmaIncidence, in.sigmaWavelengthNm};
        const auto p = PC::prism(in.nPrism, in.nMedium, in.apex, a);
        const auto deviationError = propagateUncertainty(values, sigmas, [&in](const std::vector<double>& v) {
            const double deltaLambda = v[4] - in.wavelengthNm;
            const double nPrism = v[0] + in.dnPrismDlambda * deltaLambda;
            const double nMedium = v[1] + in.dnMediumDlambda * deltaLambda;
            const auto result = PC::prism(nPrism, nMedium, v[2], v[3]);
            return result.valid && !result.entryTir && !result.exitTir ? result.deviation : kNaN;
        });
        const bool valid = p.valid && !p.exitTir && !p.entryTir;
        s.rows.push_back({double(a), valid ? p.deviation : kNaN, in.sigmaIncidence,
                          valid && deviationError.valid ? deviationError.standardUncertainty : kNaN});
    }
    const auto current = PC::prism(in.nPrism, in.nMedium, in.apex, in.incidence);
    if (current.valid && !current.entryTir && !current.exitTir) {
        const auto deviationError = propagateUncertainty(
            {in.nPrism, in.nMedium, in.apex, in.incidence, in.wavelengthNm},
            {in.sigmaNPrism, in.sigmaNMedium, in.sigmaApex, in.sigmaIncidence, in.sigmaWavelengthNm},
            [&in](const std::vector<double>& v) {
                const double deltaLambda = v[4] - in.wavelengthNm;
                const double nPrism = v[0] + in.dnPrismDlambda * deltaLambda;
                const double nMedium = v[1] + in.dnMediumDlambda * deltaLambda;
                const auto result = PC::prism(nPrism, nMedium, v[2], v[3]);
                return result.valid && !result.entryTir && !result.exitTir ? result.deviation : kNaN;
            });
        if (deviationError.valid) s.analysis.emplace_back("Current deviation δ", measured(current.deviation, deviationError.standardUncertainty, "°"));
    }
    return s;
}

// ------------------------------------------------------------------------------ double slit
Report doubleSlit(const SlitInput& in, const ReportContext& ctx) {
    std::vector<FieldIssue> issues;
    if (!std::isfinite(in.wavelengthNm) || in.wavelengthNm <= 0.0) issues.push_back({FieldIds::wavelength, "Enter λ > 0 nm.", "λ"});
    if (!std::isfinite(in.separationMm) || in.separationMm <= 0.0) issues.push_back({FieldIds::slitSeparation, "Enter d > 0 mm.", "d"});
    if (!std::isfinite(in.screenM) || in.screenM <= 0.0) issues.push_back({FieldIds::screenDistance, "Enter L > 0 m.", "L"});
    if (in.order < 0) issues.push_back({FieldIds::order, "Enter m ≥ 0.", "m"});
    validateSigma(issues, FieldIds::sigmaWavelength, in.sigmaWavelengthNm, "σ(λ)");
    validateSigma(issues, FieldIds::sigmaSlitSeparation, in.sigmaSeparationMm, "σ(d)");
    validateSigma(issues, FieldIds::sigmaScreenDistance, in.sigmaScreenM, "σ(L)");
    if (!issues.empty()) {
        std::vector<std::string> lines;
        for (const auto& i : issues) {
            if (i.field == FieldIds::slitSeparation) { lines.push_back("d = " + NF::fixed(in.separationMm, 3) + " mm"); lines.push_back("β = λL / d → UNDEFINED"); }
            else if (i.field == FieldIds::wavelength) lines.push_back("λ = " + num(in.wavelengthNm) + " nm");
            else if (i.field == FieldIds::screenDistance) lines.push_back("L = " + num(in.screenM) + " m");
            else lines.push_back("m = " + std::to_string(in.order));
        }
        const std::string cond = issues.front().message.substr(6, issues.front().message.size() - 6 - 1);     // "d > 0 mm" without "Enter " and "."
        lines.push_back("ACTION: ENTER " + (issues.front().field == FieldIds::slitSeparation ? std::string("d > 0") : cond));
        const std::string label = issues.front().label;
        return ReportSupport::pausedReport("E_INPUT_RANGE", issues, lines,
                            {"Calculation paused / " + (issues.front().field == FieldIds::slitSeparation ? std::string("d must be > 0") : label + " out of range"),
                             (issues.front().field == FieldIds::slitSeparation ? "Enter a positive slit separation." : "Enter a valid value.") +
                             std::string(" No new result has been calculated.\n\nDiagram, chart and retained results show %RUN%, not the current inputs. "
                                         "Restore ") + label + " or edit the highlighted field to continue."}, ctx);
    }

    const double lam = in.wavelengthNm * 1e-9, d = in.separationMm * 1e-3, L = in.screenM;
    const double beta = PC::fringeSpacing(lam, L, d);
    const double ym = PC::fringePosition(in.order, lam, L, d);
    const double intensity = PC::relativeIntensity(ym, lam, L, d);
    const std::vector<double> slitValues{in.wavelengthNm, in.separationMm, in.screenM};
    const std::vector<double> slitSigmas{in.sigmaWavelengthNm, in.sigmaSeparationMm, in.sigmaScreenM};
    const auto betaEstimate = propagateUncertainty(slitValues, slitSigmas, [](const std::vector<double>& v) {
        return (v[0] * 1e-9 * v[2] / (v[1] * 1e-3)) * 1e3; // β in millimetres
    });
    const auto fringeEstimate = propagateUncertainty(slitValues, slitSigmas, [order = in.order](const std::vector<double>& v) {
        return order * v[0] * 1e-9 * v[2] / (v[1] * 1e-3) * 1e3; // y_m in millimetres
    });
    const auto intensityEstimate = propagateUncertainty(slitValues, slitSigmas, [yMm = ym * 1e3](const std::vector<double>& v) {
        return PC::relativeIntensity(yMm * 1e-3, v[0] * 1e-9, v[2], v[1] * 1e-3);
    });
    const bool uncertain = anyUncertainty({in.sigmaWavelengthNm, in.sigmaSeparationMm, in.sigmaScreenM});

    Report rep;
    LcdRegister r1;
    r1.lines = {"01  λ = " + NF::scientific(lam) + " m", "02  d = " + NF::scientific(d) + " m", "03  β = λL / d", "    β = " + NF::fixed(beta, 6) + " m"};
    r1.resultLabel = "FRINGE SPACING · β";
    r1.resultValue = fx(beta * 1e3) + " mm";
    if (uncertain && betaEstimate.valid) {
        r1.resultValue += " ± " + num(betaEstimate.standardUncertainty) + " mm";
        r1.extraLines.push_back("σ(β) = " + num(betaEstimate.standardUncertainty) + " mm (1σ)");
    }
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = ordinal(in.order);
    const std::string m = std::to_string(in.order);
    static const char* sub[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
    const std::string yName = in.order < 10 ? std::string("y") + sub[in.order] : "y(m)";
    r2.lines = {"m = " + m, yName + " = mλL / d", yName + " = " + NF::signedFixed(ym * 1e3) + " mm",
                "Δ = " + m + "λ = " + NF::fixed(in.order * in.wavelengthNm, 1) + " nm", "I(" + yName + ") / I₀ = " + fx(intensity)};
    if (uncertain && fringeEstimate.valid)
        r2.extraLines.push_back("σ(" + yName + ") = " + num(fringeEstimate.standardUncertainty) + " mm (1σ)");
    if (uncertain && intensityEstimate.valid)
        r2.extraLines.push_back("σ(I/I₀) = " + num(intensityEstimate.standardUncertainty) + " (1σ)");
    rep.registers.push_back(r2);
    rep.notices.push_back({"Model assumptions", "Equal slit intensities, coherent sources and L ≫ d. The plot shows normalized interference only; no diffraction envelope. "
                           "Entered uncertainties are independent 1σ standard uncertainties.", false});
    rep.headline = "β = " + fx(beta * 1e3) + " mm";
    if (uncertain && betaEstimate.valid) rep.headline += " ± " + num(betaEstimate.standardUncertainty) + " mm";
    std::vector<std::string> retainedExtra{"m = " + m + " · " + yName + " = " + NF::signedFixed(ym * 1e3) + " mm"};
    if (uncertain && fringeEstimate.valid)
        retainedExtra.push_back("σ(" + yName + ") = " + num(fringeEstimate.standardUncertainty) + " mm (1σ)");
    rep.retained = retainedRegister({"λ = " + NF::fixed(in.wavelengthNm, 1) + " nm", "d = " + NF::fixed(in.separationMm, 3) + " mm · L = " + NF::fixed(in.screenM, 3) + " m"},
                                    "RETAINED FRINGE SPACING · β", r1.resultValue, std::move(retainedExtra));
    rep.parameters = {{"Wavelength λ", measured(in.wavelengthNm, in.sigmaWavelengthNm, "nm")},
                      {"Slit separation d", measured(in.separationMm, in.sigmaSeparationMm, "mm")},
                      {"Screen distance L", measured(in.screenM, in.sigmaScreenM, "m")}, {"Fringe order m", m}};
    return rep;
}

DataSeries doubleSlitSweep(const SlitInput& in) {
    DataSeries s{"double_slit_sweep", "Double-slit intensity",
                 NF::general(in.wavelengthNm, 5) + " nm · d = " + NF::general(in.separationMm, 4) + " mm · L = " + NF::general(in.screenM, 5) + " m",
                 "ideal coherent sources; σ(I/I₀) is an independent-input 1σ uncertainty; no diffraction envelope",
                 {"y (mm)", "I / I₀", "σ(I / I₀)"}, {}, {}};
    s.analysis = {{"Input wavelength λ", measured(in.wavelengthNm, in.sigmaWavelengthNm, "nm")},
                  {"Input slit separation d", measured(in.separationMm, in.sigmaSeparationMm, "mm")},
                  {"Input screen distance L", measured(in.screenM, in.sigmaScreenM, "m")}};
    const double lam = in.wavelengthNm * 1e-9, d = in.separationMm * 1e-3, L = in.screenM;
    const std::vector<double> values{in.wavelengthNm, in.separationMm, in.screenM};
    const std::vector<double> sigmas{in.sigmaWavelengthNm, in.sigmaSeparationMm, in.sigmaScreenM};
    for (int k = -10; k <= 10; ++k) {
        const double y = k * 1e-3;
        const double intensity = d > 0 ? PC::relativeIntensity(y, lam, L, d) : kNaN;
        const auto error = propagateUncertainty(values, sigmas, [y](const std::vector<double>& v) {
            return PC::relativeIntensity(y, v[0] * 1e-9, v[2], v[1] * 1e-3);
        });
        s.rows.push_back({double(k), intensity, std::isfinite(intensity) && error.valid ? error.standardUncertainty : kNaN});
    }
    const double betaMm = PC::fringeSpacing(lam, L, d) * 1e3;
    const auto betaError = propagateUncertainty(values, sigmas, [](const std::vector<double>& v) {
        return v[0] * 1e-9 * v[2] / (v[1] * 1e-3) * 1e3;
    });
    if (betaError.valid) s.analysis.emplace_back("Fringe spacing β", measured(betaMm, betaError.standardUncertainty, "mm"));
    return s;
}

// ------------------------------------------------------------------------------ thick lens
namespace {

Notice info(const std::string& body) { return {"Interpretation", body, false}; }
Notice limit(const std::string& body) { return {"Model limits", body, false}; }

/// Cauchy pairs of the Schott glasses the app names (A + B/λ², λ in µm). Only the shape of the curve matters
/// here: the visible spread between red and blue is what the "Spectrum" tab shows.
struct GlassCurve { const char* name; double a, b; };
GlassCurve glassFor(double n) {
    if (n < 1.45) return {"acrylic", 1.4750, 0.0140};
    if (n < 1.62) return {"crown glass", 1.5046, 0.0042};
    if (n < 1.75) return {"flint glass", 1.6030, 0.0100};
    return {"heavy flint", 1.7150, 0.0150};
}

} // namespace

Report thickLens(const ThickLensInput& in, const ReportContext& ctx) {
    ThickLensInput ti;                                          // the physics lives in ThickLens.cpp
    ti.n = in.n; ti.nMedium = in.nMedium; ti.r1 = in.r1; ti.r2 = in.r2;
    ti.thickness = in.thickness; ti.diameter = in.diameter; ti.objectDistance = in.objectDistance;
    const TL::Result a = TL::analyse(ti);
    if (!a.valid)
        return ReportSupport::pausedReport("E_INPUT_RANGE", {{FieldIds::thickness, a.error, "t"}},
                            {"n = " + num(in.n) + "   R₁ = " + num(in.r1) + " cm", "R₂ = " + num(in.r2) + " cm   t = " + num(in.thickness) + " cm",
                             "ACTION: " + a.error},
                            {"Calculation paused / element not usable", a.error + "\n\nDiagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    Report rep;
    const GlassCurve glass = glassFor(in.n);
    const std::string fText = NF::signedFixed(a.focalLength, 4) + " cm";

    if (in.spectrum) {                                          // ---- third tab: the same lens in three colours
        struct Line { const char* name; double nm; };
        const Line lines[] = {{"Red F", 656.3}, {"Green d", 587.6}, {"Blue C", 486.1}};
        double lo = 1e9, hi = -1e9;
        std::vector<std::string> rows;
        std::string headline;
        for (const Line& line : lines) {
            ThickLensInput s = ti;
            s.n = TL::cauchyIndex(glass.a, glass.b, line.nm);
            const TL::Result r = TL::analyse(s);
            if (!r.valid) continue;
            lo = std::min(lo, r.focalLength); hi = std::max(hi, r.focalLength);
            rows.push_back(std::string(line.name) + " " + NF::fixed(line.nm, 1) + " nm: n = " + NF::fixed(s.n, 5) +
                           "  f = " + NF::signedFixed(r.focalLength, 4) + " cm");
            if (headline.empty()) headline = "f(" + NF::fixed(line.nm, 1) + " nm) = " + NF::signedFixed(r.focalLength, 4) + " cm";
        }
        const double spread = hi - lo;
        LcdRegister r1;
        r1.title = "CAUCHY / " + std::string(glass.name);
        r1.mode = "OK";
        r1.lines = {"n(λ) = A + B/λ²", "A = " + NF::fixed(glass.a, 4) + "   B = " + NF::fixed(glass.b, 4)};
        r1.lines.insert(r1.lines.end(), rows.begin(), rows.end());
        r1.resultLabel = "FOCAL SPREAD f(red) − f(blue)";
        r1.resultValue = NF::fixed(std::fabs(spread), 4) + " cm";
        LcdRegister r2;
        r2.title = "ABBE NUMBER / DISPERSION";
        const double nD = TL::cauchyIndex(glass.a, glass.b, 587.6);
        const double nF = TL::cauchyIndex(glass.a, glass.b, 486.1), nC = TL::cauchyIndex(glass.a, glass.b, 656.3);
        const double abbe = std::fabs(nF - nC) > 1e-12 ? (nD - 1.0) / (nF - nC) : 0.0;
        r2.lines = {"V_d = (n_d − 1) / (n_F − n_C)", "n_d = " + NF::fixed(nD, 5) + "   n_F = " + NF::fixed(nF, 5), "n_C = " + NF::fixed(nC, 5),
                    "small V = strong dispersion"};
        r2.resultLabel = "ABBE NUMBER Vd";
        r2.resultValue = NF::fixed(abbe, 2);
        rep.registers = {r1, r2};
        rep.notices = {info("Blue light sees a larger index, so it is bent more: the blue focus sits closer to the lens and the image edge is fringed. The spread here is " +
                                      NF::fixed(std::fabs(spread), 4) + " cm for f ≈ " + NF::fixed(std::fabs(a.focalLength), 3) + " cm — real crown-glass lenses are far better corrected than that."),
                       limit("Cauchy's two-term law is a fit over the visible range. Achromats pair a crown and a flint element; this screen models one element.")};
        rep.headline = headline.empty() ? "f not defined" : headline;
        rep.retained = retainedRegister({"n = " + num(in.n) + " · R₁ = " + num(in.r1) + " · R₂ = " + num(in.r2), "t = " + num(in.thickness) + " cm"},
                                        "RETAINED FOCAL SPREAD", NF::fixed(std::fabs(spread), 4) + " cm", {"Vd = " + NF::fixed(abbe, 2)});
        rep.parameters = {{"n", num(in.n)}, {"R₁", num(in.r1) + " cm"}, {"R₂", num(in.r2) + " cm"}, {"t", num(in.thickness) + " cm"}, {"mode", "spectrum"}};
        return rep;
    }

    LcdRegister r1;
    r1.title = "THICK-LENS POWER / SI";
    r1.lines = {"P = (n−1)(1/R₁ − 1/R₂) + (n−1)² t /(n R₁R₂)",
                "P = " + NF::fixed(a.power, 6) + " cm⁻¹", "f = 1/P = " + fText,
                "ELEMENT: " + a.element, "BFL = " + NF::signedFixed(a.backFocal, 4) + " cm   FFL = " + NF::signedFixed(a.frontFocal, 4) + " cm"};
    r1.resultLabel = "THICK-LENS FOCAL LENGTH · f";
    r1.resultValue = fText;
    LcdRegister r2;
    r2.title = "PRINCIPAL PLANES / cm";
    r2.lines = {"V₁H₁ = " + NF::signedFixed(a.h1, 4) + "   V₂H₂ = " + NF::signedFixed(a.h2, 4),
                "dₒ from H₁ = " + NF::signedFixed(a.objectFromH1, 4),
                a.atInfinity ? std::string("dᵢ = ∞ (object in the focal plane)") : "dᵢ from H₂ = " + NF::signedFixed(a.imageFromH2, 4) + " cm",
                a.atInfinity ? std::string("m = undefined") : "m = −dᵢ/dₒ = " + NF::signedFixed(a.magnification, 4),
                std::string("IMAGE: ") + (a.real ? "REAL" : "VIRTUAL") + (a.atInfinity ? "" : std::string(a.upright ? " · UPRIGHT" : " · INVERTED"))};
    r2.resultLabel = "MAGNIFICATION · m";
    r2.resultValue = a.atInfinity ? std::string("—") : NF::signedFixed(a.magnification, 4);
    rep.registers = {r1, r2};

    const std::string saSign = a.longitudinalSa < 0 ? "marginal rays cross " + NF::fixed(std::fabs(a.longitudinalSa), 4) + " cm in front of the paraxial focus"
                                                    : "marginal rays cross " + NF::fixed(a.longitudinalSa, 4) + " cm behind the paraxial focus";
    rep.notices = {info("The principal planes are imaginary surfaces inside the glass, " + NF::signedFixed(a.h1, 3) +
                                  " cm / " + NF::signedFixed(a.h2, 3) + " cm from the vertices: all distances are measured from them, which is why the thick lens does not obey the thin-lens equation from the vertices. Spherical aberration (third order, D = " +
                                  num(in.diameter) + " cm): " + a.saClass + " — " + saSign + "."),
                   limit("Paraxial power and third-order aberration only: no coma, astigmatism or field curvature. The aberration assumes the stop at the lens and an object at infinity; the aperture is " + num(in.diameter) + " cm.")};
    rep.headline = a.atInfinity ? "dᵢ → ∞" : "dᵢ = " + NF::signedFixed(a.imageFromH2, 4) + " cm";
    rep.footnote = "H₁: front principal plane · H₂: rear · BFL " + NF::signedFixed(a.backFocal, 3) + " cm · f/# " + NF::fixed(a.fNumber, 2);
    rep.statusText = a.converging ? (a.saClass == "NEGLIGIBLE" ? "Well-corrected aperture" : "Converging element · " + a.saClass) : "Diverging element · virtual focus";
    rep.retained = retainedRegister({"n = " + num(in.n) + " · R₁ = " + num(in.r1) + " cm · R₂ = " + num(in.r2) + " cm", "t = " + num(in.thickness) + " cm · D = " + num(in.diameter) + " cm"},
                                    "RETAINED THICK-LENS FOCAL LENGTH", fText, {"ΔSA = " + NF::signedFixed(a.longitudinalSa, 4) + " cm · " + a.saClass});
    rep.parameters = {{"Element", a.element}, {"n", num(in.n)}, {"R₁", num(in.r1) + " cm"}, {"R₂", num(in.r2) + " cm"},
                      {"t", num(in.thickness) + " cm"}, {"D", num(in.diameter) + " cm"}, {"dₒ", num(in.objectDistance) + " cm"}};
    return rep;
}

DataSeries thickLensSweep(const ThickLensInput& in) {
    ThickLensInput ti;
    ti.n = in.n; ti.nMedium = in.nMedium; ti.r1 = in.r1; ti.r2 = in.r2;
    ti.thickness = in.thickness; ti.diameter = in.diameter; ti.objectDistance = in.objectDistance;
    DataSeries s{"thick_lens_sweep", "Thick-lens sweep",
                 "n = " + num(in.n) + " · R₁ = " + num(in.r1) + " cm · R₂ = " + num(in.r2) + " cm · t = " + num(in.thickness) + " cm",
                 "dᵢ measured from the rear principal plane H₂", {"dₒ (cm)", "dᵢ (cm)", "m"}, {}};
    for (const auto& [dObj, dImg] : TL::imageCurve(ti, 12)) {
        const TL::Result r = TL::analyse([&] { ThickLensInput c = ti; c.objectDistance = dObj; return c; }());
        s.rows.push_back({dObj, dImg, r.valid ? r.magnification : kNaN});
    }
    return s;
}

} // namespace phy104::ExperimentReports
