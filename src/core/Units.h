#pragma once
#include <optional>
#include <string>

namespace phy104::Units {

/// Factor that converts a value written in `unit` to SI: "nm" -> 1e-9, "mm" -> 1e-3, "µm" -> 1e-6, "deg" or "°" -> π/180,
/// "kHz" -> 1e3. An empty unit is dimensionless (1). Unknown units give nullopt.
std::optional<double> toSI(const std::string& unit);

} // namespace phy104::Units
