#pragma once
#include "Models.h"
#include "Repositories.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace phy104 {

/// Builds the formula catalogue. For every formula it reads the variable names
/// straight from the equation text and joins them with the variable dictionary
/// (units, display symbols, descriptions) and with the constants (default values).
class FormulaEngine {
public:
    FormulaEngine(std::unique_ptr<IFormulaRepository> formulas,
                  const IVariableRepository& variables,
                  const IConstantsRepository& constants);

    const std::vector<Formula>& formulas() const { return m_formulas; }
    const Formula* findById(const std::string& id) const;
    std::vector<std::string> chapters() const;
    std::vector<const Formula*> byChapter(const std::string& chapter) const;

    /// Data problems found while loading (e.g. a variable missing from the dictionary).
    const std::vector<std::string>& warnings() const { return m_warnings; }

    /// Checks one '=' and that both sides parse. Returns an error text or nullopt.
    static std::optional<std::string> validate(const Formula& formula);

private:
    std::vector<Formula> m_formulas;
    std::vector<std::string> m_warnings;
};

} // namespace phy104
