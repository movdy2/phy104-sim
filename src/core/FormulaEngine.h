#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include "models.h"
#include "Repositories.h"
//------------------------------------------------

using std::unique_ptr;
using std::string;
using std::vector;
using std::optional;

namespace phy104 {

/// Builds the formula catalogue. For every formula it reads the variable names
/// straight from the equation text and joins them with the variable dictionary
/// (units, display symbols, descriptions) and with the constants (default values).
class FormulaEngine {
public:
    FormulaEngine(unique_ptr<IFormulaRepository> formulas,
                  const IVariableRepository& variables,
                  const IConstantsRepository& constants);

    const vector<Formula>& formulas() const { return m_formulas; }
    const Formula* findById(const string& id) const;
    vector<string> chapters() const;
    vector<const Formula*> byChapter(const string& chapter) const;

    /// Data problems found while loading (e.g. a variable missing from the dictionary).
    const vector<string>& warnings() const { return m_warnings; }

    /// Checks one '=' and that both sides parse. Returns an error text or nullopt.
    static optional<string> validate(const Formula& formula);

private:
    vector<Formula> m_formulas;
    vector<string> m_warnings;
};

} // namespace phy104