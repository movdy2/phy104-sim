#pragma once
#include <string>
#include <map>
#include <vector>
using std::string;
using std::vector;
using std::map;
//------------------------------------------------

namespace phy104 {

/// One variable of a formula. `id` is the ASCII name used inside equations
/// (e.g. "thetaa"); `display` is the pretty symbol shown to the user (e.g. "θ_a").
struct Variable {
    string id;
    string display;
    string name;
    string unit; // "m", "deg", "rad", "" ... (SI; angles may be deg or rad)
    string description;
    string defaultValue; // pre-filled text, e.g. the value of c (filled from constants)
};

/// A formula. Only the equation TEXT is stored; the solver works with that text,
/// so no hand-written rearrangements are needed. `variables` is filled by FormulaEngine.
struct Formula {
    string id;
    string name;
    string equation; // "na*sin(thetaa)=nb*sin(thetab)"
    string latex;
    string chapter;
    map<string, string> unitOverrides; // e.g. {"phi","deg"} for Malus' law
    vector<Variable> variables;

    const Variable* findVariable(const string& id) const {
        for (const auto& v : variables)
            if (v.id == id) return &v;
        return nullptr;
    }
};

struct PhysicsConstant {
    string id; // ASCII name usable in expressions: "epsilon0"
    string symbol; // pretty symbol: "ε₀"
    string name;
    double value = 0.0;
    string unit;
    string description;
};

struct NoteEntry {
    string id;
    string chapter;
    string type; // "summary" | "concept" | "exam" | "equation"
    string title;
    vector<string> content;
    vector<string> equations;
};

/// A row of keys on the virtual keyboard.
struct KeyboardGroup {
    string name;
    vector<string> keys; // text inserted when pressed (e.g. "sin(")
};

} // namespace phy104