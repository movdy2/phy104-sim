#pragma once
// Everything the experiment screens display, as plain data: LCD registers, notices, inline field
// errors, data series. No Qt - the pages only paint what these functions return.
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace phy104 {

/// One Casio-style calculation register.
struct LcdRegister {
    std::string title = "CALCULATION REGISTER";
    std::string mode = "DEG";           // shown as "● DEG" (or "● OK")
    bool resultFirst = false;           // big value right under the title (error code registers)
    std::vector<std::string> lines;
    std::string resultLabel;            // small caption above the big value ("" = none)
    std::string resultValue;
    std::vector<std::string> extraLines;// lines below the big value
    std::string footer = "MEM: 01        PREC: 4 DP";
    bool isError = false;
};

struct Notice {
    std::string title;
    std::string body;
    bool error = false;                 // red card
};

/// A problem with one input field (shown under the field in red).
struct FieldIssue {
    std::string field;                  // "d", "theta1", "f" ... (the page maps it to a widget)
    std::string message;                // "Enter d > 0 mm."
    std::string label;                  // short name for "Restore d = 0.250 mm"
};

struct Report {
    bool valid = true;                  // false: nothing was calculated
    bool paused = false;                // true: invalid input, the page shows the paused pattern
    std::string errorCode;              // "E_INPUT_RANGE", "E_FOCAL_PLANE", "E_EXIT_TIR" ...
    std::vector<LcdRegister> registers;
    std::vector<Notice> notices;
    std::vector<FieldIssue> issues;
    std::string headline;               // "θ₂ = 48.5904°"
    std::string footnote;
    std::string statusText;             // status bar: "Valid outcome: TIR"
    LcdRegister retained;               // compact summary used as the "LAST VALID" register later
    std::vector<std::pair<std::string, std::string>> parameters;   // name -> "value unit"
};

/// A table of numbers (NaN = "not defined", e.g. total internal reflection).
struct DataSeries {
    std::string id;
    std::string name;
    std::string description;            // "Glass → air · n₁ = 1.5000 · n₂ = 1.0000"
    std::string note;                   // "sin θ is dimensionless"
    std::vector<std::string> headers;
    std::vector<std::vector<double>> rows;
};

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct RefractionInput {
    double n1 = 1.5, n2 = 1.0, theta1 = 30.0;
    bool tirMode = false;               // true: the single-case TIR screen
    std::string name1 = "Crown glass", name2 = "Air";
};
struct ImagingInput {
    double f = 10.0, s = 30.0, h = 3.0; // cm. f is SIGNED: + converging lens / concave mirror, - diverging / convex
    bool mirrorMode = false;
};
struct PrismInput {
    double nPrism = 1.5, nMedium = 1.0, apex = 60.0, incidence = 48.5904, wavelengthNm = 589.3;
    bool dispersion = false;
};
struct SlitInput {
    double wavelengthNm = 532.0, separationMm = 0.25, screenM = 1.5;
    int order = 2;
};

/// What the page knows about the previous valid result (for the paused pattern).
struct ReportContext {
    const Report* lastValid = nullptr;
    std::string runLabel;               // "RUN 021" or ""
};

namespace ExperimentReports {

Report refraction(const RefractionInput& in, const ReportContext& ctx = {});     // both modes
Report imaging(const ImagingInput& in, const ReportContext& ctx = {});
Report prism(const PrismInput& in, const ReportContext& ctx = {});
Report doubleSlit(const SlitInput& in, const ReportContext& ctx = {});

/// The comparison angle used for case B when the user drives case A.
double defaultTirAngle(double n1, double n2);

DataSeries refractionSweep(const RefractionInput& in);      // θ₁ 0..35°, θ₂, sin θ₁, sin θ₂
DataSeries tirSweep(const RefractionInput& in);             // θ₁ 0..90°, θ₂ (empty beyond θc)
DataSeries imagingSweep(const ImagingInput& in);            // dₒ, dᵢ, m
DataSeries prismSweep(const PrismInput& in);                // i₁ 35..65°, δ
DataSeries doubleSlitSweep(const SlitInput& in);            // y (mm), I/I₀

struct LinearFit { double slope = 0, intercept = 0, r2 = 0; bool ok = false; };
LinearFit linearFit(const std::vector<double>& x, const std::vector<double>& y);

/// "DIMINISHED (¼×)", "ENLARGED (2.5×)", "SAME SIZE (1×)"
std::string sizeDescription(double magnification);

} // namespace ExperimentReports
} // namespace phy104
