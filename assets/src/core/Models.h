#pragma once
#include <map>
#include <string>
#include <vector>

namespace phy104 {

/// One variable of a formula.
struct Variable {
    std::string id;
    std::string display;
    std::string name;
    std::string unit;
    std::string latex;
    std::string description;
    std::string defaultValue;
};

/// A formula. Only the equation TEXT is stored.
struct Formula {
    std::string id;
    std::string name;
    std::string equation;
    std::string latex;
    std::string chapter;
    std::string noSolutionHint;
    std::map<std::string, std::string> unitOverrides;
    std::vector<Variable> variables;

    const Variable* findVariable(const std::string& id) const {
        for (const auto& v : variables)
            if (v.id == id) return &v;
        return nullptr;
    }
};

/// A course chapter.
struct Chapter {
    std::string id;
    std::string title;
    int order = 0;
};

/// Physical constant.
struct PhysicsConstant {
    std::string id;
    std::string symbol;
    std::string name;
    std::string category;      // <--- أضفنا هذا الحقل
    double value = 0.0;
    std::string unit;
    std::string description;
};

/// A row of keys on the virtual keyboard.
struct KeyboardGroup {
    std::string name;
    std::vector<std::string> keys;
};

/// A transparent medium.
struct OpticalMedium {
    std::string id;
    std::string name;
    double n = 1.0;
    double cauchyA = 0.0;
    double cauchyB = 0.0;
};

/// A monochromatic light source.
struct LightSource {
    std::string id;
    std::string name;
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

/// A formula topic of the reference page.
struct ReferenceTopic {
    std::string title;
    std::vector<std::string> equations;
    std::string note;
};

/// A note entry from the JSON reference.
struct NoteEntry {
    std::string id;
    std::string chapter;
    std::string type;
    std::string title;
    std::vector<std::string> content;
    std::vector<std::string> equations;
};

} // namespace phy104