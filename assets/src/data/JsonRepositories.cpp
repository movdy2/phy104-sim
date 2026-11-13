#include "JsonRepositories.h"
#include "JsonLoader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <stdexcept>

using std::string;
using std::vector;
using std::runtime_error;

namespace phy104 {
namespace {

string str(const QJsonObject& o, const char* key, bool required = true) {
    if (!o.contains(key)) {
        if (required) throw runtime_error(string("Missing key \"") + key + "\" in JSON object");
        return {};
    }
    const QJsonValue val = o.value(key);
    if (!val.isString()) {
        if (required) throw runtime_error(string("Key \"") + key + "\" is not a string in JSON object");
        return {};
    }
    return val.toString().toStdString();
}

vector<string> strList(const QJsonValue& v) {
    vector<string> out;
    if (!v.isArray()) return out;
    for (const auto& item : v.toArray()) {
        if (item.isString()) out.push_back(item.toString().toStdString());
    }
    return out;
}

QJsonArray requireArray(const QJsonObject& root, const char* key) {
    if (!root.contains(key) || !root.value(key).isArray()) {
        throw runtime_error(string("Missing or invalid array \"") + key + "\" in JSON root");
    }
    return root.value(key).toArray();
}

} // namespace

vector<Formula> JsonFormulaRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "formulas");
    vector<Formula> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        Formula f;
        f.id = str(o, "id");
        f.name = str(o, "name");
        f.equation = str(o, "equation");
        f.latex = str(o, "latex", false);
        f.chapter = str(o, "chapter");
        f.noSolutionHint = str(o, "noSolutionHint", false);
        const QJsonObject units = o.value("units").toObject();
        for (auto it = units.begin(); it != units.end(); ++it) {
            f.unitOverrides[it.key().toStdString()] = it.value().toString().toStdString();
        }
        result.push_back(std::move(f));
    }
    return result;
}

vector<Variable> JsonVariableRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "variables");
    vector<Variable> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        Variable v;
        v.id = str(o, "id");
        v.display = str(o, "symbol");
        v.name = str(o, "name");
        v.unit = str(o, "unit", false);
        v.description = str(o, "description", false);
        v.latex = str(o, "latex", false);
        result.push_back(std::move(v));
    }
    return result;
}

vector<Chapter> JsonChapterRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "chapters");
    vector<Chapter> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        Chapter c;
        c.id = str(o, "id");
        c.title = str(o, "title");
        c.order = o.value("order").toInt();
        result.push_back(std::move(c));
    }
    return result;
}

vector<PhysicsConstant> JsonConstantsRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "constants");
    vector<PhysicsConstant> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        PhysicsConstant c;
        c.id = str(o, "id");
        c.symbol = str(o, "symbol");
        c.name = str(o, "name");
        c.category = str(o, "category", false); // <--- قراءة التصنيف الجديد
        c.value = o.value("value").toDouble();
        c.unit = str(o, "unit", false);
        c.description = str(o, "description", false);
        result.push_back(std::move(c));
    }
    return result;
}

OpticsPresets JsonPresetsRepository::load() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    OpticsPresets presets;
    for (const auto& item : root.value("media").toArray()) {
        const QJsonObject o = item.toObject();
        presets.media.push_back({str(o, "id"), str(o, "name"), o.value("n").toDouble(1.0),
                                 o.value("cauchyA").toDouble(), o.value("cauchyB").toDouble()});
    }
    for (const auto& item : root.value("sources").toArray()) {
        const QJsonObject o = item.toObject();
        presets.sources.push_back({str(o, "id"), str(o, "name"), o.value("wavelengthNm").toDouble(550.0)});
    }
    return presets;
}

vector<ReferenceTopic> JsonReferenceRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "topics");
    vector<ReferenceTopic> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        ReferenceTopic t;
        t.title = str(o, "title");
        t.equations = strList(o.value("equations"));
        t.note = str(o, "note", false);
        result.push_back(std::move(t));
    }
    return result;
}

string JsonReferenceRepository::unitDiscipline() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonObject o = root.value("unitDiscipline").toObject();
    return str(o, "title", false) + "\n" + str(o, "text", false);
}

vector<NoteEntry> JsonSummaryRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "notes");
    vector<NoteEntry> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        NoteEntry n;
        n.id = str(o, "id");
        n.chapter = str(o, "chapter");
        n.type = str(o, "type");
        n.title = str(o, "title");
        n.content = strList(o.value("content"));
        n.equations = strList(o.value("equations"));
        result.push_back(std::move(n));
    }
    return result;
}

vector<KeyboardGroup> JsonKeyboardRepository::loadAll() const {
    const QJsonObject root = JsonLoader::loadObject(m_path);
    const QJsonArray arr = requireArray(root, "groups");
    vector<KeyboardGroup> result;
    result.reserve(static_cast<size_t>(arr.size()));
    for (const auto& item : arr) {
        const QJsonObject o = item.toObject();
        KeyboardGroup g;
        g.name = str(o, "name");
        g.keys = strList(o.value("keys"));
        result.push_back(std::move(g));
    }
    return result;
}

bool JsonSessionStore::save(const SessionLog& log) const {
    QJsonArray runs;
    for (const RunRecord& r : log.runs()) {
        runs.append(QJsonObject{
            {"id", r.id},
            {"experiment", QString::fromStdString(r.experiment)},
            {"description", QString::fromStdString(r.description)},
            {"result", QString::fromStdString(r.result)},
            {"valid", r.valid}
        });
    }

    QJsonArray equations;
    for (const string& e : log.equations()) {
        equations.append(QString::fromStdString(e));
    }

    QJsonArray snapshots;
    for (const auto& [name, snap] : log.snapshots()) {
        QJsonArray params;
        for (const auto& [k, v] : snap.parameters) {
            params.append(QJsonObject{{"name", QString::fromStdString(k)}, {"value", QString::fromStdString(v)}});
        }
        QJsonObject snapObj{
            {"experiment", QString::fromStdString(snap.experiment)},
            {"headline", QString::fromStdString(snap.headline)},
            {"parameters", params},
            {"notesContent", QString::fromStdString(snap.notesContent)}
        };
        snapshots.append(snapObj);
    }

    const QJsonObject root{
        {"format", "phy104-session"},
        {"version", 4},
        {"session", log.sessionNumber()},
        {"notes", QString::fromStdString(log.notes())},
        {"runs", runs},
        {"snapshots", snapshots},
        {"equations", equations}
    };

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool JsonSessionStore::load(SessionLog& log) const {
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || doc.object().value("format").toString() != "phy104-session") return false;

    const QJsonObject root = doc.object();

    vector<RunRecord> runs;
    for (const auto& item : root.value("runs").toArray()) {
        const QJsonObject o = item.toObject();
        RunRecord r;
        r.id = o.value("id").toInt();
        r.experiment = str(o, "experiment", false);
        r.description = str(o, "description", false);
        r.result = str(o, "result", false);
        r.valid = o.value("valid").toBool(true);
        runs.push_back(std::move(r));
    }
    log.restoreRuns(std::move(runs));
    log.setNotes(root.value("notes").toString().toStdString());

    std::map<string, Snapshot> snapshots;
    for (const auto& v : root.value("snapshots").toArray()) {
        const QJsonObject o = v.toObject();
        Snapshot s;
        s.experiment = str(o, "experiment", false);
        s.headline = str(o, "headline", false);
        for (const auto& p : o.value("parameters").toArray()) {
            const QJsonObject po = p.toObject();
            s.parameters.emplace_back(str(po, "name", false), str(po, "value", false));
        }
        s.notesContent = str(o, "notesContent", false);
        snapshots[s.experiment] = std::move(s);
    }
    log.restoreSnapshots(std::move(snapshots));
    log.setSessionNumber(root.value("session").toInt(log.sessionNumber()));

    vector<string> equations;
    for (const auto& e : root.value("equations").toArray()) {
        equations.push_back(e.toString().toStdString());
    }
    log.setEquations(std::move(equations));

    return true;
}

} // namespace phy104