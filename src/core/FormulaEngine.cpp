#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <iomanip>
#include "ExpressionEvaluator.h"
#include "FormulaEngine.h"
//------------------------------------------------

using std::string;
using std::vector;
using std::unordered_map;
using std::unique_ptr;
using std::find;
using std::map;
using std::optional;
using std::nullopt;

namespace phy104 {

FormulaEngine::FormulaEngine(unique_ptr<IFormulaRepository> formulaRepo,
                             const IVariableRepository& variableRepo,
                             const IConstantsRepository& constantsRepo)
    : m_formulas(formulaRepo->loadAll()) {

    map<string, Variable> dictionary;
    for (auto& v : variableRepo.loadAll()) dictionary[v.id] = v;
    
    map<string, PhysicsConstant> constants;
    for (auto& c : constantsRepo.loadAll()) constants[c.id] = c;

    for (auto& formula : m_formulas) {
        if (auto problem = validate(formula)) {
            m_warnings.push_back(*problem);
        }

        const auto eq = formula.equation.find('=');
        for (const auto& id : ExpressionEvaluator::identifiers(formula.equation)) {
            Variable var;
            const auto d = dictionary.find(id);
            if (d != dictionary.end()) {
                var = d->second;
            } else {
                var.id = var.display = var.name = id;
                m_warnings.push_back("Variable '" + id + "' (formula " + formula.id +
                                     ") is missing from variables.json");
            }
            const auto c = constants.find(id);
            if (c != constants.end()) {
                std::ostringstream text;
                text << std::setprecision(12) << c->second.value;
                var.defaultValue = text.str();
            }
            formula.variables.push_back(std::move(var));
        }
        (void)eq;
    }
}

const Formula* FormulaEngine::findById(const string& id) const {
    for (const auto& f : m_formulas)
        if (f.id == id) return &f;
    return nullptr;
}

vector<string> FormulaEngine::chapters() const {
    vector<string> result;
    for (const auto& f : m_formulas) {
        if (find(result.begin(), result.end(), f.chapter) == result.end()) {
            result.push_back(f.chapter);
        }
    }
    return result;
}

vector<const Formula*> FormulaEngine::byChapter(const string& chapter) const {
    vector<const Formula*> result;
    for (const auto& f : m_formulas) {
        if (f.chapter == chapter) {
            result.push_back(&f);
        }
    }
    return result;
}

optional<string> FormulaEngine::validate(const Formula& formula) {
    const auto eq = formula.equation.find('=');
    if (eq == string::npos || formula.equation.find('=', eq + 1) != string::npos)
        return "Formula '" + formula.id + "': equation must contain exactly one '='";

    VariableMap vars;
    for (const auto& id : ExpressionEvaluator::identifiers(formula.equation)) vars[id] = 0.7;
    try {
        ExpressionEvaluator::evaluate(formula.equation.substr(0, eq), vars);
        ExpressionEvaluator::evaluate(formula.equation.substr(eq + 1), vars);
    } catch (const std::exception& e) {
        return "Formula '" + formula.id + "': " + e.what();
    }
    return nullopt;
}

} // namespace phy104