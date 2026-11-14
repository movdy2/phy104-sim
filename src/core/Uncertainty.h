#pragma once

#include <functional>
#include <vector>

namespace phy104 {

/// A value with its combined standard uncertainty (one standard deviation).
struct UncertainEstimate {
    double value = 0.0;
    double standardUncertainty = 0.0;
    bool valid = false;
};

/// First-order propagation for independent input uncertainties, using centred finite differences.
/// Inputs and uncertainties must be ordered identically; uncertainties are non-negative 1σ values.
UncertainEstimate propagateUncertainty(
    const std::vector<double>& values,
    const std::vector<double>& standardUncertainties,
    const std::function<double(const std::vector<double>&)>& evaluate);

/// Weighted least-squares line fit for measurements with known, independent y standard uncertainties.
struct WeightedLinearFit {
    double slope = 0.0;
    double intercept = 0.0;
    double slopeUncertainty = 0.0;
    double interceptUncertainty = 0.0;
    double chiSquared = 0.0;
    double r2 = 0.0;
    int degreesOfFreedom = 0;
    bool ok = false;
};

WeightedLinearFit weightedLinearFit(const std::vector<double>& x,
                                    const std::vector<double>& y,
                                    const std::vector<double>& yStandardUncertainties);

} // namespace phy104
