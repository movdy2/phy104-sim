#pragma once
// The six wave-optics screens of the second Figma batch: Newton's rings, Michelson interferometer,
// thin film, diffraction grating, single slit, Malus / Brewster. Pure C++: registers, notices and chart
// data are returned as plain structs; the pages only paint them.
#include "ExperimentReports.h"

#include <string>
#include <utility>
#include <vector>

namespace phy104 {

/// What the chart panel draws: a polyline, an optional highlighted sample and its axes.
struct ChartData {
    std::string yLabel, xLabel, legend;
    double xMin = 0, xMax = 1, yMin = 0, yMax = 1;
    int xTicks = 4, yTicks = 2, xDecimals = 1, yDecimals = 1;
    std::vector<std::pair<double, double>> curve;
    std::vector<std::pair<double, double>> marker;      // 0 or 1 points
};

/// A report plus its chart (the diagram is drawn by the page from the same inputs).
struct ModelReport {
    Report report;
    ChartData chart;
};

/// `transmitted` picks the second mode tab: reflected (false) or transmitted (true) light.
struct NewtonInput    { double lambdaNm = 600, radiusM = 1.0, order = 4, n = 1.0; bool transmitted = false; };
/// `fringes` and `piezo` are the second tab: count/tune fringes instead of stepping the mirror by Δx.
struct MichelsonInput { double lambdaNm = 632.8, displacementUm = 0.75, n = 1.0, visibility = 1.0; bool fringes = false, piezo = false; };
/// `transmission` picks the second mode tab: reflected (false) or transmitted (true) light.
struct ThinFilmInput  { double lambdaNm = 600, n = 1.5, thicknessNm = 100; bool transmission = false; };
struct GratingInput   { double linesPerCm = 5000, lambdaNm = 600, order = 1, slits = 20, incidenceDeg = 0; };
struct SingleSlitInput{ double widthMm = 0.1, lambdaNm = 500, screenM = 1.0; };
struct PolarizationInput { double incidentIntensity = 100, angleDeg = 60, n1 = 1.0, n2 = 1.5; bool unpolarized = true; };

namespace WaveExperiments {

ModelReport newtonRings(const NewtonInput& in, const ReportContext& ctx = {});
ModelReport michelson(const MichelsonInput& in, const ReportContext& ctx = {});
ModelReport thinFilm(const ThinFilmInput& in, const ReportContext& ctx = {});
ModelReport grating(const GratingInput& in, const ReportContext& ctx = {});
ModelReport singleSlit(const SingleSlitInput& in, const ReportContext& ctx = {});
ModelReport polarization(const PolarizationInput& in, const ReportContext& ctx = {});

/// Orders m with a real angle: |m λ/d + sin θi| <= 1, in increasing order.
std::vector<int> allowedGratingOrders(double linesPerCm, double lambdaNm, double incidenceDeg);
/// I/Imax of an ideal N-slit grating without envelope at angle θ (degrees).
double gratingIntensity(double thetaDeg, double linesPerCm, double lambdaNm, double slits, double incidenceDeg = 0);
/// (sin β / β)², β = π a sin θ / λ; 1 at β = 0
double singleSlitIntensity(double thetaDeg, double widthMm, double lambdaNm);

} // namespace WaveExperiments
} // namespace phy104
