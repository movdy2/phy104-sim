#include "Uncertainty.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace phy104 {
namespace {

UncertainEstimate invalidEstimate(double value = std::numeric_limits<double>::quiet_NaN()) {
    return {value, std::numeric_limits<double>::quiet_NaN(), false};
}

} // namespace

UncertainEstimate propagateUncertainty(
    const std::vector<double>& values,
    const std::vector<double>& standardUncertainties,
    const std::function<double(const std::vector<double>&)>& evaluate) {
    if (!evaluate || values.empty() || values.size() != standardUncertainties.size()) return invalidEstimate();
    for (double value : values) if (!std::isfinite(value)) return invalidEstimate();
    for (double uncertainty : standardUncertainties)
        if (!std::isfinite(uncertainty) || uncertainty < 0.0) return invalidEstimate();

    const double nominal = evaluate(values);
    if (!std::isfinite(nominal)) return invalidEstimate(nominal);

    double combined = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double sigma = standardUncertainties[i];
        if (sigma == 0.0) continue;

        const double step = std::max({std::fabs(values[i]) * 1e-6, sigma * 1e-3, 1e-8});
        std::vector<double> plus = values;
        std::vector<double> minus = values;
        plus[i] += step;
        minus[i] -= step;
        const double high = evaluate(plus);
        const double low = evaluate(minus);
        double sensitivity = std::numeric_limits<double>::quiet_NaN();
        if (std::isfinite(high) && std::isfinite(low))
            sensitivity = (high - low) / (2.0 * step);
        else if (std::isfinite(high))
            sensitivity = (high - nominal) / step;
        else if (std::isfinite(low))
            sensitivity = (nominal - low) / step;
        if (!std::isfinite(sensitivity)) return invalidEstimate(nominal);
        combined = std::hypot(combined, sensitivity * sigma);
        if (!std::isfinite(combined)) return invalidEstimate(nominal);
    }
    return {nominal, combined, true};
}

WeightedLinearFit weightedLinearFit(const std::vector<double>& x,
                                    const std::vector<double>& y,
                                    const std::vector<double>& yStandardUncertainties) {
    WeightedLinearFit fit;
    const std::size_t count = x.size();
    if (count < 2 || y.size() != count || yStandardUncertainties.size() != count) return fit;

    double sumW = 0.0, sumWX = 0.0, sumWY = 0.0, sumWXX = 0.0, sumWXY = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double sigma = yStandardUncertainties[i];
        if (!std::isfinite(x[i]) || !std::isfinite(y[i]) || !std::isfinite(sigma) || sigma <= 0.0) return fit;
        const double weight = 1.0 / (sigma * sigma);
        sumW += weight;
        sumWX += weight * x[i];
        sumWY += weight * y[i];
        sumWXX += weight * x[i] * x[i];
        sumWXY += weight * x[i] * y[i];
    }

    const double determinant = sumW * sumWXX - sumWX * sumWX;
    if (!std::isfinite(determinant) || determinant <= std::numeric_limits<double>::epsilon() * sumW * sumWXX) return fit;

    fit.slope = (sumW * sumWXY - sumWX * sumWY) / determinant;
    fit.intercept = (sumWXX * sumWY - sumWX * sumWXY) / determinant;
    fit.slopeUncertainty = std::sqrt(sumW / determinant);
    fit.interceptUncertainty = std::sqrt(sumWXX / determinant);
    fit.degreesOfFreedom = static_cast<int>(count) - 2;

    const double weightedMean = sumWY / sumW;
    double weightedTotal = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double weight = 1.0 / (yStandardUncertainties[i] * yStandardUncertainties[i]);
        const double residual = y[i] - (fit.slope * x[i] + fit.intercept);
        fit.chiSquared += weight * residual * residual;
        const double centred = y[i] - weightedMean;
        weightedTotal += weight * centred * centred;
    }
    fit.r2 = weightedTotal > 0.0 ? 1.0 - fit.chiSquared / weightedTotal
                                  : (fit.chiSquared <= std::numeric_limits<double>::epsilon() ? 1.0 : 0.0);
    fit.ok = std::isfinite(fit.slope) && std::isfinite(fit.intercept) &&
             std::isfinite(fit.slopeUncertainty) && std::isfinite(fit.interceptUncertainty) &&
             std::isfinite(fit.chiSquared) && std::isfinite(fit.r2);
    return fit;
}

} // namespace phy104
