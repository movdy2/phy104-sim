#include "FormulaEngine.h"
#include "ExpressionEvaluator.h"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>

using std::string;
using std::vector;
using std::map;
using std::unique_ptr;
using std::optional;
using std::nullopt;

namespace phy104 {

FormulaEngine::FormulaEngine(unique_ptr<IFormulaRepository> formulaRepo,
                             const IVariableRepository& variableRepo,
                             const IConstantsRepository& constantsRepo)
    : m_formulas(formulaRepo->loadAll()) {

    // 1. بناء قاموس المتغيرات (id -> Variable) من ملف variables.json
    map<string, Variable> dictionary;
    for (const auto& v : variableRepo.loadAll()) dictionary[v.id] = v;

    // 2. بناء قاموس الثوابت (id -> PhysicsConstant) من ملف constants.json
    map<string, PhysicsConstant> constants;
    for (const auto& c : constantsRepo.loadAll()) constants[c.id] = c;

    // 3. معالجة كل معادلة
    for (auto& formula : m_formulas) {
        // التحقق من صحة المعادلة (يوجد '=' واحد فقط، ويمكن حساب الطرفين)
        if (auto problem = validate(formula)) {
            m_warnings.push_back(*problem);
        }

        // استخراج أسماء المتغيرات من نص المعادلة (مثلاً "n1*sin(theta1)" -> ["n1", "theta1"])
        for (const auto& id : ExpressionEvaluator::identifiers(formula.equation)) {
            Variable var;

            // ربط المتغير بقاموس variables.json لجلب الاسم والوصف والوحدة
            const auto d = dictionary.find(id);
            if (d != dictionary.end()) {
                var = d->second;
            } else {
                var.id = var.display = var.name = id;
                m_warnings.push_back("Variable '" + id + "' (formula " + formula.id + ") is missing from variables.json");
            }

            // إذا كان المتغير يمثل ثابتاً فيزيائياً (مثل c أو epsilon0)، نضع قيمته كقيمة افتراضية
            const auto c = constants.find(id);
            if (c != constants.end()) {
                std::ostringstream text;
                text << std::setprecision(12) << c->second.value;
                var.defaultValue = text.str();
            }

            formula.variables.push_back(std::move(var));
        }
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
        if (std::find(result.begin(), result.end(), f.chapter) == result.end()) {
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
    // التحقق من وجود '=' واحد فقط
    const auto eq = formula.equation.find('=');
    if (eq == string::npos || formula.equation.find('=', eq + 1) != string::npos)
        return "Formula '" + formula.id + "': equation must contain exactly one '='";

    // اختبار المعادلة بقيم تجريبية (0.7) للتأكد من عدم وجود أخطاء نحوية
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
