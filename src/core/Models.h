#pragma once
// Plain data structures shared by all layers (no Qt in here).
#include <map>
#include <string>
#include <vector>

namespace phy104 {

/// One variable of a formula. `id` is the ASCII name used inside equations
/// (e.g. "thetaa"); `display` is the pretty symbol shown to the user (e.g. "θ_a").
struct Variable {
    std::string id;
    std::string display;
    std::string name;
    std::string unit;          // "m", "deg", "rad", "" ...  (SI; angles may be deg or rad)
    std::string description;
    std::string latex;         // LaTeX symbol, e.g. \\theta_{b}
    std::string defaultValue;  // pre-filled text, e.g. the value of c (filled from constants)
};

/// A formula. Only the equation TEXT is stored; the solver works with that text,
/// so no hand-written rearrangements are needed. `variables` is filled by FormulaEngine.
struct Formula {
    std::string id;
    std::string name;
    std::string equation;      // "na*sin(thetaa)=nb*sin(thetab)"
    std::string latex;
    std::string chapter;       // chapter id, see Chapter / chapters.json
    std::string noSolutionHint;// shown when no solution exists (e.g. total internal reflection)
    std::map<std::string, std::string> unitOverrides;  // e.g. {"phi","deg"} for Malus' law
    std::vector<Variable> variables;

    const Variable* findVariable(const std::string& variableId) const {
        for (const auto& v : variables)
            if (v.id == variableId) return &v;
        return nullptr;
    }
};

/// A course chapter. Formulas refer to it by `id`; `order` decides the sort order.
struct Chapter {
    std::string id;
    std::string title;
    int order = 0;
};

struct PhysicsConstant {
    std::string id;            // ASCII name usable in expressions: "epsilon0"
    std::string symbol;        // pretty symbol: "ε₀"
    std::string name;
    double value = 0.0;
    std::string unit;
    std::string description;
};

/// A key of the Figma "Scientific input" keyboard. `control` is one of @shift @left @right @back @clear (then the rest is empty).
struct ScientificKey {
    std::string legend;          // what the key shows
    std::string latex;           // what it inserts ("\\sqrt{}": the caret goes into the first {} or ())
    std::string shiftLegend, shiftLatex;   // optional: capital Greek letter while SHIFT is latched
    std::string control;
};
struct ScientificPage {
    std::string name;            // "Greek symbols"
    std::vector<std::vector<ScientificKey>> rows;
};

/// A row of keys on the virtual keyboard.
struct KeyboardGroup {
    std::string name;
    std::vector<std::string> keys;       // text inserted when pressed (e.g. "sin(")
};

} // namespace phy104

namespace phy104 {

/// A transparent medium (air, glass ...). n is the index at 589.3 nm; the Cauchy terms
/// give n(λ) = A + B/λ² (λ in µm) for the prism's dispersion mode.
struct OpticalMedium {
    std::string id;
    std::string name;          // "Glass · crown"
    double n = 1.0;
    double cauchyA = 0.0;
    double cauchyB = 0.0;
};

/// A monochromatic light source preset for the double-slit experiment.
struct LightSource {
    std::string id;
    std::string name;          // "Green laser · 532 nm"
    double wavelengthNm = 550.0;
};

struct OpticsPresets {
    std::vector<OpticalMedium> media;
    std::vector<LightSource> sources;

    const OpticalMedium* findMedium(const std::string& id) const {
        for (const auto& m : media) if (m.id == id) return &m;
        return nullptr;
    }
};

/// A formula topic of the "Formulas & Notes" page.
struct ReferenceTopic {
    std::string title;
    std::vector<std::string> equations;
    std::string note;
};

} // namespace phy104
