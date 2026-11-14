#include "ExperimentReports.h"
#include "NumberFormat.h"
#include "PhysicsCalculator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

using std::string;
using std::vector;
using std::size_t;
using std::round;
using std::fabs;
using std::min;
using std::max;
using std::asin;
using std::sin;

namespace phy104::ExperimentReports {
namespace NF = NumberFormat;
namespace PC = PhysicsCalculator;
namespace {

constexpr double kDeg = std::numbers::pi / 180.0;

// تنسيق الأرقام: 6 أرقام معنوية (general) أو 4 منازل عشرية (fixed)
string num(double v) { return NF::general(v, 6); }
string fx(double v) { return NF::fixed(v, 4); }

string lower(string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }

string replaceAll(string text, const string& from, const string& to) {
    for (size_t pos = 0; (pos = text.find(from, pos)) != string::npos; pos += to.size()) text.replace(pos, from.size(), to);
    return text;
}

// ============================================================
// بناء "حالة التوقف" (Paused) عند إدخال خاطئ
// ============================================================
Report pausedReport(const string& code, vector<FieldIssue> issues, vector<string> lines,
                    Notice notice, const ReportContext& ctx) {
    Report r;
    r.valid = false;
    r.paused = true;
    r.errorCode = code;
    r.issues = std::move(issues);

    LcdRegister cur;
    cur.title = "CURRENT INPUT / PAUSED";
    cur.resultFirst = true;
    cur.resultValue = code;
    cur.lines = std::move(lines);
    cur.footer = "NO NEW RESULT / MEMORY RETAINED";
    cur.isError = true;
    r.registers.push_back(cur);

    const string run = lower(ctx.runLabel);
    const bool haveLast = ctx.lastValid && ctx.lastValid->valid && !ctx.lastValid->retained.title.empty();
    if (haveLast) {
        LcdRegister kept = ctx.lastValid->retained;
        kept.title = "LAST VALID" + (ctx.runLabel.empty() ? string() : " / " + ctx.runLabel);
        kept.footer = "STALE / NOT CURRENT INPUTS";
        r.registers.push_back(kept);
        r.headline = ctx.lastValid->headline;
    }
    notice.error = true;
    notice.body = replaceAll(notice.body, "%RUN%", ctx.runLabel.empty() ? "the last valid result" : run);
    r.notices.push_back(notice);
    r.statusText = code + " · " + (haveLast ? "last valid " + (ctx.runLabel.empty() ? string("result") : run) + " retained" : "nothing to retain");
    return r;
}

// ============================================================
// ترتيب الشق المزدوج (Central, First, Second, ...)
// ============================================================
string ordinal(int m) {
    switch (m) {
    case 0: return "CENTRAL MAXIMUM";
    case 1: return "FIRST BRIGHT FRINGE";
    case 2: return "SECOND BRIGHT FRINGE";
    case 3: return "THIRD BRIGHT FRINGE";
    default: return "BRIGHT FRINGE m = " + std::to_string(m);
    }
}

// ============================================================
// بناء "السجل المحتفظ به" (Retained Register) لآخر نتيجة صحيحة
// ============================================================
LcdRegister retainedRegister(vector<string> lines, const string& label, const string& value,
                             vector<string> extra) {
    LcdRegister r;
    r.title = "LAST VALID";
    r.lines = std::move(lines);
    r.resultLabel = label;
    r.resultValue = value;
    r.extraLines = std::move(extra);
    return r;
}

} // namespace

// ============================================================
// وصف حجم الصورة (مكبرة، مصغرة، نفس الحجم)
// ============================================================
string sizeDescription(double magnification) {
    const double a = fabs(magnification);
    if (fabs(a - 1.0) < 1e-4) return "SAME SIZE (1×)";
    string text;
    if (a < 1.0) {
        static const std::pair<double, const char*> fractions[] = {{0.125, "⅛"}, {0.25, "¼"}, {1.0 / 3, "⅓"}, {0.375, "⅜"}, {0.5, "½"},
                                                                    {0.625, "⅝"}, {2.0 / 3, "⅔"}, {0.75, "¾"}, {0.875, "⅞"}};
        for (const auto& [value, glyph] : fractions)
            if (fabs(a - value) < 5e-4) { text = glyph; break; }
        if (text.empty()) text = NF::general(a, 3);
        return "DIMINISHED (" + text + "×)";
    }
    return "ENLARGED (" + NF::general(a, 3) + "×)";
}

// ============================================================
// زاوية TIR الافتراضية (للحالة B في تجربة الانكسار)
// ============================================================
double defaultTirAngle(double n1, double n2) {
    const auto crit = PC::criticalAngleDeg(n1, n2);
    if (!crit) return 50.0;
    return *crit < 50.0 ? 50.0 : min(85.0, *crit + 10.0);
}

namespace {
Report tirReport(const RefractionInput& in, const ReportContext& ctx);
} // namespace

// ============================================================
// تجربة الانكسار (Refraction)
// ============================================================
Report refraction(const RefractionInput& in, const ReportContext& ctx) {
    vector<FieldIssue> issues;
    if (in.n1 <= 0.0) issues.push_back({"n1", "Enter n₁ > 0.", "n₁"});
    if (in.n2 <= 0.0) issues.push_back({"n2", "Enter n₂ > 0.", "n₂"});
    if (in.theta1 < 0.0 || in.theta1 >= 90.0) issues.push_back({"theta1", "Enter 0° ≤ θ₁ < 90°.", "θ₁"});
    if (!issues.empty()) {
        const string first = issues.front().label;
        return pausedReport("E_INPUT_RANGE", issues,
                            {"n₁ = " + fx(in.n1) + "   n₂ = " + fx(in.n2), "θ₁ = " + fx(in.theta1) + "°", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + first + " out of range",
                             "Refractive indices must be positive and 0° ≤ θ₁ < 90°. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);
    }
    if (in.tirMode) return tirReport(in, ctx);

    const double thetaA = in.theta1;
    const double thetaB = defaultTirAngle(in.n1, in.n2);
    const auto crit = PC::criticalAngleDeg(in.n1, in.n2);

    Report rep;
    {
        const auto ref = PC::refraction(in.n1, in.n2, thetaA);
        const double sinA = sin(thetaA * kDeg);
        const double sin2 = in.n1 * sinA / in.n2;
        LcdRegister r;
        r.lines = {"01  n₁ sin θ₁ = n₂ sin θ₂",
                   "02  sin θ₂ = " + num(in.n1 / in.n2) + " × " + num(round(sinA * 1e4) / 1e4),
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
        }
        rep.registers.push_back(r);
        rep.retained = retainedRegister({"n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "θ₁ = " + NF::fixed(in.theta1, 2) + "°"},
                                        "RETAINED REFRACTION ANGLE · θ₂", r.resultValue, {});
    }
    {
        const double sinB = in.n1 * sin(thetaB * kDeg) / in.n2;
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
        rep.registers.push_back(r);
    }
    rep.notices.push_back({"Physical limit, not a solver error",
                           "For θ₁ > θc, no real refracted angle exists. TIR is reported instead of evaluating asin outside [−1, 1].", false});
    rep.parameters = {{"Mode", "Refraction"}, {"n₁", num(in.n1)}, {"n₂", num(in.n2)}, {"θ₁ (incidence)", num(in.theta1) + " °"}};
    return rep;
}

namespace {

// ============================================================
// تجربة TIR (الحالة الواحدة)
// ============================================================
Report tirReport(const RefractionInput& in, const ReportContext&) {
    const auto crit = PC::criticalAngleDeg(in.n1, in.n2);
    const double th = in.theta1;
    const double sin2 = in.n1 * sin(th * kDeg) / in.n2;
    const bool tir = crit && th > *crit && sin2 > 1.0;

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
    r1.resultLabel = "PHYSICAL OUTCOME";
    r1.resultValue = tir ? "TIR" : (crit ? "PARTIAL" : "NO TIR");
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = "REFLECTION REGISTER";
    r2.mode = "OK";
    r2.lines.push_back("θᵣ = θ₁ = " + fx(th) + "°");
    if (tir) {
        r2.lines.push_back("TRANSMISSION: NONE");
        r2.lines.push_back("STATUS: VALID / TIR");
        r2.resultLabel = "ABOVE CRITICAL BY";
        r2.resultValue = fx(th - *crit) + "°";
    } else {
        r2.lines.push_back("TRANSMISSION: θ₂ = " + fx(asin(min(1.0, sin2)) / kDeg) + "°");
        r2.lines.push_back(string("STATUS: VALID / ") + (crit ? "REFRACTION" : "NO TIR"));
        if (crit) { r2.resultLabel = "BELOW CRITICAL BY"; r2.resultValue = fx(*crit - th) + "°"; }
    }
    rep.registers.push_back(r2);

    const string n1 = in.name1, n2 = in.name2;
    if (tir)
        rep.notices.push_back({"Valid physical outcome, not an error",
                               "All physical rays remain in " + lower(n1) + ". Above θc there is no propagating transmitted ray; the sweep stops at θ₂ = 90° at the critical limit.", false});
    else
        rep.notices.push_back({"Refraction occurs, no TIR",
                               crit ? "θ₁ is below θc = " + fx(*crit) + "°, so part of the light is transmitted into " + lower(n2) + " and part is reflected."
                                    : "TIR requires n₁ > n₂. Going into a denser medium the ray always refracts.", false});
    rep.headline = tir ? "θc = " + fx(*crit) + "° · TIR" : "θ₂ = " + fx(asin(min(1.0, sin2)) / kDeg) + "°";
    rep.statusText = tir ? "Valid outcome: TIR" : "Valid outcome: refraction";
    rep.retained = retainedRegister({"n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "θ₁ = " + NF::fixed(th, 2) + "°"},
                                    "RETAINED OUTCOME", r1.resultValue, {});
    rep.parameters = {{"Mode", "TIR"}, {"n₁", num(in.n1)}, {"n₂", num(in.n2)}, {"θ₁ (incidence)", num(th) + " °"}};
    return rep;
}

} // namespace

// ============================================================
// مسح الانكسار (Refraction Sweep)
// ============================================================
DataSeries refractionSweep(const RefractionInput& in) {
    DataSeries s{"refraction_sweep", "Refraction sweep",
                 in.name1 + " → " + in.name2 + " · n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "sin θ is dimensionless",
                 {"θ₁ (°)", "θ₂ (°)", "sin θ₁", "sin θ₂"}, {}};
    for (int a = 0; a <= 35; a += 5) {
        const double sin1 = sin(a * kDeg), sin2 = in.n1 * sin1 / in.n2;
        const double th2 = fabs(sin2) > 1.0 ? kNaN : asin(sin2) / kDeg;
        s.rows.push_back({double(a), th2, sin1, sin2});
    }
    return s;
}

DataSeries tirSweep(const RefractionInput& in) {
    DataSeries s{"critical_sweep", "Critical-angle sweep",
                 in.name1 + " → " + in.name2 + " · n₁ = " + fx(in.n1) + " · n₂ = " + fx(in.n2), "θ₂ is empty where TIR occurs",
                 {"θ₁ (°)", "θ₂ (°)"}, {}};
    for (int a = 0; a <= 90; a += 5) {
        const double sin2 = in.n1 * sin(a * kDeg) / in.n2;
        s.rows.push_back({double(a), fabs(sin2) > 1.0 ? kNaN : asin(sin2) / kDeg});
    }
    return s;
}

// ============================================================
// الانحدار الخطي (Linear Fit)
// ============================================================
LinearFit linearFit(const vector<double>& x, const vector<double>& y) {
    LinearFit f;
    const size_t n = x.size();
    if (n < 2 || y.size() != n) return f;
    double sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0;
    for (size_t i = 0; i < n; ++i) { sx += x[i]; sy += y[i]; sxx += x[i] * x[i]; sxy += x[i] * y[i]; syy += y[i] * y[i]; }
    const double den = n * sxx - sx * sx;
    if (fabs(den) < 1e-300) return f;
    f.slope = (n * sxy - sx * sy) / den;
    f.intercept = (sy - f.slope * sx) / n;
    const double ssTot = syy - sy * sy / n;
    double ssRes = 0;
    for (size_t i = 0; i < n; ++i) { const double e = y[i] - (f.slope * x[i] + f.intercept); ssRes += e * e; }
    f.r2 = ssTot < 1e-300 ? 1.0 : 1.0 - ssRes / ssTot;
    f.ok = true;
    return f;
}

// ============================================================
// تجربة العدسات والمرايا (Lenses & Mirrors)
// ============================================================
Report imaging(const ImagingInput& in, const ReportContext& ctx) {
    vector<FieldIssue> issues;
    if (in.f == 0.0) issues.push_back({"f", "Enter f ≠ 0.", "f"});
    if (in.s <= 0.0) issues.push_back({"s", "Enter dₒ > 0.", "dₒ"});
    if (in.h <= 0.0) issues.push_back({"h", "Enter hₒ > 0.", "hₒ"});
    if (!issues.empty())
        return pausedReport("E_INPUT_RANGE", issues,
                            {"f = " + num(in.f) + " cm   dₒ = " + num(in.s) + " cm", "hₒ = " + num(in.h) + " cm", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + issues.front().label + " out of range",
                             "Enter a non-zero focal length and positive object distance and height. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const auto img = PC::imageFormation(in.s, in.f);
    if (img.atInfinity)
        return pausedReport("E_FOCAL_PLANE", {{"s", "Object in the focal plane: move dₒ away from f.", "dₒ"}},
                            {"dₒ = f = " + num(in.s) + " cm", "dᵢ → ∞  (image at infinity)", "m → undefined", "ACTION: MOVE dₒ AWAY FROM f"},
                            {"Guarded case / E_FOCAL_PLANE",
                             "Comparison: dₒ = f = " + num(in.s) + " cm. Image at infinity; finite dᵢ and m suppressed. Move the object away from the focal plane to calculate a finite image.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const double m = img.magnification;
    Report rep;
    LcdRegister r1;
    const string fText = in.f < 0 ? "(" + num(in.f) + ")" : num(in.f);
    r1.lines = {"01  1/f = 1/dₒ + 1/dᵢ", "02  1/dᵢ = 1/" + fText + " - 1/" + num(in.s)};
    const string invDi = NF::fixed(1.0 / in.f - 1.0 / in.s, 6);
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
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = "IMAGE PROPERTIES";
    if (in.mirrorMode) {
        r2.mode = "OK";
        r2.lines = {"m = -dᵢ/dₒ = " + NF::signedFixed(m), "hᵢ = m × hₒ = " + NF::signedFixed(m * in.h) + " cm",
                    string("IMAGE: ") + (img.real ? "REAL" : "VIRTUAL"), string("ORIENTATION: ") + (img.upright ? "UPRIGHT" : "INVERTED"),
                    "SIZE: " + sizeDescription(m)};
    } else {
        r2.lines = {"hᵢ = m × hₒ", "hᵢ = " + fx(m * in.h) + " cm", string("IMAGE: ") + (img.real ? "REAL" : "VIRTUAL"),
                    string("ORIENTATION: ") + (img.upright ? "UPRIGHT" : "INVERTED"), "MIRROR: SAME RESULTS"};
    }
    rep.registers.push_back(r2);

    if (in.mirrorMode) {
        rep.notices.push_back(img.imageDistance < 0
            ? Notice{"Negative distance ≠ invalid input",
                     "dᵢ < 0 locates the virtual image behind the mirror" + string(in.f < 0 ? ", between P and F" : "") +
                     ". Positive magnification means upright; |m| < 1 means diminished.", false}
            : Notice{"Real image in front of the mirror",
                     "dᵢ > 0 locates the real image in front of the mirror, on the same side as the object. Negative magnification means inverted.", false});
        rep.footnote = "P: pole · F: " + NF::general(in.f, 5) + " cm · C: " + NF::general(2 * in.f, 5) + " cm";
    } else {
        rep.notices.push_back({"Guarded case / E_FOCAL_PLANE",
                               "Comparison: dₒ = f = " + num(in.f) + " cm. Image at infinity; finite dᵢ and m suppressed. Move the object away from the focal plane to calculate a finite image.", true});
    }
    rep.headline = "dᵢ = " + NF::signedFixed(img.imageDistance) + " cm";
    rep.statusText = img.real ? "Real image · positive distances" : "Virtual image · negative distances are valid";
    rep.retained = retainedRegister({"f = " + NF::signedFixed(in.f, 2) + " cm · dₒ = " + NF::fixed(in.s, 2) + " cm", "hₒ = " + NF::fixed(in.h, 2) + " cm"},
                                    "RETAINED IMAGE DISTANCE · dᵢ", NF::signedFixed(img.imageDistance) + " cm",
                                    {"m = " + fx(m) + " · hᵢ = " + fx(m * in.h) + " cm"});
    rep.parameters = {{"Element", string(in.mirrorMode ? "Mirror" : "Thin lens") + (in.f < 0 ? " (diverging / convex)" : " (converging / concave)")},
                      {"f", num(in.f) + " cm"}, {"dₒ", num(in.s) + " cm"}, {"hₒ", num(in.h) + " cm"}};
    return rep;
}

DataSeries imagingSweep(const ImagingInput& in) {
    char desc[96];
    std::snprintf(desc, sizeof desc, "f = %s cm · hₒ = %s cm", NF::general(in.f, 5).c_str(), NF::general(in.h, 5).c_str());
    DataSeries s{"imaging_sweep", in.mirrorMode ? "Spherical mirror sweep" : "Thin lens sweep", desc, "negative dᵢ = virtual image", {"dₒ (cm)", "dᵢ (cm)", "m"}, {}};
    const double af = fabs(in.f);
    for (int k = 0; k < 8; ++k) {
        const double so = af * (1.25 + 0.5 * k);
        const auto img = PC::imageFormation(so, in.f);
        s.rows.push_back({so, img.atInfinity ? kNaN : img.imageDistance, img.atInfinity ? kNaN : img.magnification});
    }
    return s;
}

// ============================================================
// تجربة المنشور (Prisms)
// ============================================================
Report prism(const PrismInput& in, const ReportContext& ctx) {
    vector<FieldIssue> issues;
    if (in.apex <= 0.0 || in.apex >= 180.0) issues.push_back({"apex", "Enter 0° < A < 180°.", "A"});
    if (in.incidence < 0.0 || in.incidence >= 90.0) issues.push_back({"incidence", "Enter 0° ≤ i₁ < 90°.", "i₁"});
    if (in.wavelengthNm <= 0.0) issues.push_back({"lambda", "Enter λ > 0 nm.", "λ"});
    if (!issues.empty())
        return pausedReport("E_INPUT_RANGE", issues,
                            {"A = " + num(in.apex) + "°   i₁ = " + num(in.incidence) + "°", "λ = " + num(in.wavelengthNm) + " nm", "ACTION: " + issues.front().message},
                            {"Calculation paused / " + issues.front().label + " out of range",
                             "Apex 0° < A < 180°, incidence 0° ≤ i₁ < 90°, wavelength > 0. No new result has been calculated.\n\n"
                             "Diagram, chart and retained results show %RUN%, not the current inputs."}, ctx);

    const double n = in.nPrism / in.nMedium;
    const auto p = PC::prism(in.nPrism, in.nMedium, in.apex, in.incidence);
    const auto dmin = PC::minimumDeviationDeg(in.nPrism, in.nMedium, in.apex);

    Report rep;
    if (p.exitTir || p.entryTir) {
        LcdRegister r;
        r.lines = {"r₁ = " + fx(p.r1) + "°", "r₂ = A − r₁ = " + fx(p.r2) + "°", "n sin r₂ = " + fx(n * sin(p.r2 * kDeg)) + " > 1"};
        r.resultLabel = "PHYSICAL OUTCOME";
        r.resultValue = "TIR";
        rep.registers.push_back(r);
        rep.valid = true;                                          // a valid physical outcome, not an input error
        rep.errorCode = "E_EXIT_TIR";
        rep.notices.push_back({"Exit-face validation", "n sin r₂ > 1: the exit ray undergoes TIR. The solver reports no emergent ray rather than an invalid angle.", false});
        rep.headline = "δ = no emergent ray";
        rep.statusText = "Valid outcome: TIR at the exit face";
        rep.retained = retainedRegister({"n = " + fx(n) + " · A = " + NF::fixed(in.apex, 2) + "°", "i₁ = " + fx(in.incidence) + "°"}, "RETAINED OUTCOME", "TIR", {});
        rep.parameters = {{"Mode", in.dispersion ? "Dispersion" : "Single ray"}, {"n (prism / medium)", fx(n)}, {"Apex A", num(in.apex) + " °"}, {"Incidence i₁", num(in.incidence) + " °"}};
        return rep;
    }

    const bool symmetric = fabs(p.i1 - p.i2) < 0.005;
    LcdRegister r;
    if (in.dispersion) r.lines.push_back("λ = " + NF::general(in.wavelengthNm, 5) + " nm   n = " + fx(n));
    r.lines.push_back("01  r₁ = asin(sin i₁ / n)");
    r.lines.push_back("    r₁ = " + fx(p.r1) + "°");
    r.lines.push_back("02  r₂ = A - r₁ = " + num(round(p.r2 * 1e4) / 1e4) + "°");
    r.lines.push_back("03  i₂ = asin(n sin r₂)");
    r.lines.push_back("    i₂ = " + fx(p.i2) + "°");
    r.lines.push_back("04  δ = i₁ + i₂ - A");
    r.resultLabel = symmetric ? "MINIMUM DEVIATION · δmin" : "DEVIATION · δ";
    r.resultValue = fx(p.deviation) + "°";
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
    rep.headline = string(symmetric ? "δmin" : "δ") + " = " + fx(p.deviation) + "°";
    rep.retained = retainedRegister({"n = " + fx(n) + " · A = " + NF::fixed(in.apex, 2) + "°", "i₁ = " + fx(in.incidence) + "°"},
                                    "RETAINED DEVIATION · δ", fx(p.deviation) + "°", {"i₂ = " + fx(p.i2) + "°"});
    rep.parameters = {{"Mode", in.dispersion ? "Dispersion" : "Single ray"}, {"n (prism / medium)", fx(n)}, {"Apex A", num(in.apex) + " °"},
                      {"Incidence i₁", num(in.incidence) + " °"}, {"Wavelength λ", num(in.wavelengthNm) + " nm"}};
    return rep;
}

DataSeries prismSweep(const PrismInput& in) {
    DataSeries s{"prism_sweep", "Prism deviation sweep", "n = " + fx(in.nPrism / in.nMedium) + " · A = " + NF::general(in.apex, 5) + "°", "",
                 {"i₁ (°)", "δ (°)"}, {}};
    for (int a = 35; a <= 65; a += 5) {
        const auto p = PC::prism(in.nPrism, in.nMedium, in.apex, a);
        s.rows.push_back({double(a), (p.exitTir || p.entryTir || !p.valid) ? kNaN : p.deviation});
    }
    return s;
}

// ============================================================
// تجربة الشق المزدوج (Double Slit)
// ============================================================
Report doubleSlit(const SlitInput& in, const ReportContext& ctx) {
    vector<FieldIssue> issues;
    if (in.wavelengthNm <= 0.0) issues.push_back({"lambda", "Enter λ > 0 nm.", "λ"});
    if (in.separationMm <= 0.0) issues.push_back({"d", "Enter d > 0 mm.", "d"});
    if (in.screenM <= 0.0) issues.push_back({"L", "Enter L > 0 m.", "L"});
    if (in.order < 0) issues.push_back({"m", "Enter m ≥ 0.", "m"});
    if (!issues.empty()) {
        vector<string> lines;
        for (const auto& i : issues) {
            if (i.field == "d") { lines.push_back("d = " + NF::fixed(in.separationMm, 3) + " mm"); lines.push_back("β = λL / d → UNDEFINED"); }
            else if (i.field == "lambda") lines.push_back("λ = " + num(in.wavelengthNm) + " nm");
            else if (i.field == "L") lines.push_back("L = " + num(in.screenM) + " m");
            else lines.push_back("m = " + std::to_string(in.order));
        }
        const string cond = issues.front().message.substr(6, issues.front().message.size() - 6 - 1);     // "d > 0 mm" without "Enter " and "."
        lines.push_back("ACTION: ENTER " + (issues.front().field == "d" ? string("d > 0") : cond));
        const string label = issues.front().label;
        return pausedReport("E_INPUT_RANGE", issues, lines,
                            {"Calculation paused / " + (issues.front().field == "d" ? string("d must be > 0") : label + " out of range"),
                             (issues.front().field == "d" ? "Enter a positive slit separation." : "Enter a valid value.") +
                             string(" No new result has been calculated.\n\nDiagram, chart and retained results show %RUN%, not the current inputs. "
                                         "Restore ") + label + " or edit the highlighted field to continue."}, ctx);
    }

    const double lam = in.wavelengthNm * 1e-9, d = in.separationMm * 1e-3, L = in.screenM;
    const double beta = PC::fringeSpacing(lam, L, d);
    const double ym = PC::fringePosition(in.order, lam, L, d);
    const double intensity = PC::relativeIntensity(ym, lam, L, d);

    Report rep;
    LcdRegister r1;
    r1.lines = {"01  λ = " + NF::scientific(lam) + " m", "02  d = " + NF::scientific(d) + " m", "03  β = λL / d", "    β = " + NF::fixed(beta, 6) + " m"};
    r1.resultLabel = "FRINGE SPACING · β";
    r1.resultValue = fx(beta * 1e3) + " mm";
    rep.registers.push_back(r1);

    LcdRegister r2;
    r2.title = ordinal(in.order);
    const string m = std::to_string(in.order);
    static const char* sub[] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
    const string yName = in.order < 10 ? string("y") + sub[in.order] : "y(m)";
    r2.lines = {"m = " + m, yName + " = mλL / d", yName + " = " + NF::signedFixed(ym * 1e3) + " mm",
                "Δ = " + m + "λ = " + NF::fixed(in.order * in.wavelengthNm, 1) + " nm", "I(" + yName + ") / I₀ = " + fx(intensity)};
    rep.registers.push_back(r2);
    rep.notices.push_back({"Model assumptions", "Equal slit intensities, coherent sources and L ≫ d. The plot shows normalized interference only; no diffraction envelope.", false});
    rep.headline = "β = " + fx(beta * 1e3) + " mm";
    rep.retained = retainedRegister({"λ = " + NF::fixed(in.wavelengthNm, 1) + " nm", "d = " + NF::fixed(in.separationMm, 3) + " mm · L = " + NF::fixed(in.screenM, 3) + " m"},
                                    "RETAINED FRINGE SPACING · β", fx(beta * 1e3) + " mm", {"m = " + m + " · " + yName + " = " + NF::signedFixed(ym * 1e3) + " mm"});
    rep.parameters = {{"Wavelength λ", num(in.wavelengthNm) + " nm"}, {"Slit separation d", NF::general(in.separationMm, 4) + " mm"},
                      {"Screen distance L", num(in.screenM) + " m"}, {"Fringe order m", m}};
    return rep;
}

DataSeries doubleSlitSweep(const SlitInput& in) {
    DataSeries s{"double_slit_sweep", "Double-slit intensity",
                 NF::general(in.wavelengthNm, 5) + " nm · d = " + NF::general(in.separationMm, 4) + " mm · L = " + NF::general(in.screenM, 5) + " m",
                 "ideal coherent sources, no diffraction envelope", {"y (mm)", "I / I₀"}, {}};
    const double lam = in.wavelengthNm * 1e-9, d = in.separationMm * 1e-3, L = in.screenM;
    for (int k = -10; k <= 10; ++k) {
        const double y = k * 1e-3;
        s.rows.push_back({double(k), d > 0 ? PC::relativeIntensity(y, lam, L, d) : kNaN});
    }
    return s;
}

} // namespace phy104::ExperimentReports