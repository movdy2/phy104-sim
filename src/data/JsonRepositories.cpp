#include "JsonRepositories.h"
#include "JsonLoader.h"
#include <QJsonArray>
#include <QJsonObject>
#include <stdexcept>

using std::string;
using std::vector;
using std::runtime_error;

namespace phy104 {
namespace {

// Reads a string field; throws a readable error if the key is missing (helps find typos in JSON).
string str(const QJsonObject& o, const char* key, bool required = true) {
    if (!o.contains(key)) {
        if (required) throw runtime_error(string("Missing key \"") + key + "\" in JSON object");
        return {};
    }
    return o.value(key).toString().toStdString();
}

vector<string> strList(const QJsonValue& v) {
    vector<string> out;
    for (const auto& item : v.toArray()) out.push_back(item.toString().toStdString());
    return out;
}

} // namespace

vector<Formula> JsonFormulaRepository::loadAll() const {
    vector<Formula> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("formulas").toArray()) {
        const QJsonObject o = item.toObject();
        Formula f;
        f.id = str(o, "id");
        f.name = str(o, "name");
        f.equation = str(o, "equation");
        f.latex = str(o, "latex", false);
        f.chapter = str(o, "chapter");
        const QJsonObject units = o.value("units").toObject();
        for (auto it = units.begin(); it != units.end(); ++it)
            f.unitOverrides[it.key().toStdString()] = it.value().toString().toStdString();
        result.push_back(std::move(f));
    }
    return result;
}

vector<Variable> JsonVariableRepository::loadAll() const {
    vector<Variable> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("variables").toArray()) {
        const QJsonObject o = item.toObject();
        Variable v;
        v.id = str(o, "id");
        v.display = str(o, "symbol");
        v.name = str(o, "name");
        v.unit = str(o, "unit", false);
        v.description = str(o, "description", false);
        result.push_back(std::move(v));
    }
    return result;
}

vector<PhysicsConstant> JsonConstantsRepository::loadAll() const {
    vector<PhysicsConstant> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("constants").toArray()) {
        const QJsonObject o = item.toObject();
        PhysicsConstant c;
        c.id = str(o, "id");
        c.symbol = str(o, "symbol");
        c.name = str(o, "name");
        c.value = o.value("value").toDouble(); // numbers in JSON, not strings
        c.unit = str(o, "unit", false);
        c.description = str(o, "description", false);
        result.push_back(std::move(c));
    }
    return result;
}

vector<NoteEntry> JsonSummaryRepository::loadAll() const {
    vector<NoteEntry> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("notes").toArray()) {
        const QJsonObject o = item.toObject();
        result.push_back({str(o, "id"), str(o, "chapter"), str(o, "type"), str(o, "title"),
                          strList(o.value("content")), strList(o.value("equations"))});
    }
    return result;
}

vector<KeyboardGroup> JsonKeyboardRepository::loadAll() const {
    vector<KeyboardGroup> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("groups").toArray()) {
        const QJsonObject o = item.toObject();
        result.push_back({str(o, "name"), strList(o.value("keys"))});
    }
    return result;
}

} // namespace phy104