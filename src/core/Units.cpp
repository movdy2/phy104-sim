#include "Units.h"

#include <map>
#include <numbers>

namespace phy104::Units {

std::optional<double> toSI(const std::string& unitIn) {
    std::string unit = unitIn;
    const auto b = unit.find_first_not_of(" \t");
    if (b == std::string::npos) return 1.0;
    unit = unit.substr(b, unit.find_last_not_of(" \t") - b + 1);
    if (unit == "integer" || unit == "order" || unit == "slits") return 1.0;
    if (unit == "°" || unit == "deg") return std::numbers::pi / 180.0;
    if (unit == "rad") return 1.0;
    static const char* bases[] = {"Hz", "W/m²", "W/m2", "lines/cm", "m", "s", "W", "V", "J", "N", "T", "F", "H", "C", "A", "K", "g"};
    static const std::map<std::string, double> prefix = {{"p", 1e-12}, {"n", 1e-9}, {"µ", 1e-6}, {"u", 1e-6}, {"m", 1e-3}, {"c", 1e-2},
                                                          {"k", 1e3}, {"M", 1e6}, {"G", 1e9}};
    for (const char* base : bases)
        if (unit == base) return 1.0;
    for (const char* base : bases) {
        const std::string bs = base;
        if (unit.size() > bs.size() && unit.compare(unit.size() - bs.size(), bs.size(), bs) == 0) {
            const auto it = prefix.find(unit.substr(0, unit.size() - bs.size()));
            if (it != prefix.end()) return it->second;
        }
    }
    return std::nullopt;
}

} // namespace phy104::Units
