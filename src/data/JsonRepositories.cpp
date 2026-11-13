#include "JsonRepositories.h"
#include "JsonLoader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QJsonObject>

namespace phy104 {
namespace {

// Reads a string field; throws a readable error if the key is missing (helps find typos in JSON).
std::string str(const QJsonObject& o, const char* key, bool required = true) {
    if (!o.contains(key)) {
        if (required) throw std::runtime_error(std::string("Missing key \"") + key + "\" in JSON object");
        return {};
    }
    return o.value(key).toString().toStdString();
}

std::vector<std::string> strList(const QJsonValue& v) {
    std::vector<std::string> out;
    for (const auto& item : v.toArray()) out.push_back(item.toString().toStdString());
    return out;
}

} // namespace

std::vector<Formula> JsonFormulaRepository::loadAll() const {
    std::vector<Formula> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("formulas").toArray()) {
        const QJsonObject o = item.toObject();
        Formula f;
        f.id = str(o, "id");
        f.name = str(o, "name");
        f.equation = str(o, "equation");
        f.latex = str(o, "latex", false);
        f.chapter = str(o, "chapter");
        f.noSolutionHint = str(o, "noSolutionHint", false);
        const QJsonObject units = o.value("units").toObject();
        for (auto it = units.begin(); it != units.end(); ++it)
            f.unitOverrides[it.key().toStdString()] = it.value().toString().toStdString();
        result.push_back(std::move(f));
    }
    return result;
}

std::vector<Variable> JsonVariableRepository::loadAll() const {
    std::vector<Variable> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("variables").toArray()) {
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

std::vector<Chapter> JsonChapterRepository::loadAll() const {
    std::vector<Chapter> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("chapters").toArray()) {
        const QJsonObject o = item.toObject();
        result.push_back({str(o, "id"), str(o, "title"), o.value("order").toInt()});
    }
    return result;
}

std::vector<PhysicsConstant> JsonConstantsRepository::loadAll() const {
    std::vector<PhysicsConstant> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("constants").toArray()) {
        const QJsonObject o = item.toObject();
        PhysicsConstant c;
        c.id = str(o, "id");
        c.symbol = str(o, "symbol");
        c.name = str(o, "name");
        c.value = o.value("value").toDouble();          // numbers in JSON, not strings
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

std::vector<ReferenceTopic> JsonReferenceRepository::loadAll() const {
    std::vector<ReferenceTopic> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("topics").toArray()) {
        const QJsonObject o = item.toObject();
        result.push_back({str(o, "title"), strList(o.value("equations")), str(o, "note", false)});
    }
    return result;
}

std::string JsonReferenceRepository::unitDiscipline() const {
    const QJsonObject o = JsonLoader::loadObject(m_path).value("unitDiscipline").toObject();
    return str(o, "title", false) + "\n" + str(o, "text", false);
}

namespace {

const char* const kShapeNames[] = {"pen", "line", "ray", "rect", "ellipse", "text", "equation", "lens", "mirror", "prism", "interface", "axis", "normal"};

QJsonObject shapeToJson(const Shape& s) {
    QJsonArray pts;
    for (const Point& p : s.points) pts.append(QJsonArray{p.x, p.y});
    return {{"type", kShapeNames[static_cast<int>(s.type)]}, {"points", pts}, {"color", QString::fromStdString(s.color)},
            {"width", s.width}, {"dashed", s.dashed}, {"flag", s.flag}, {"text", QString::fromStdString(s.text)}};
}

Shape shapeFromJson(const QJsonObject& o) {
    Shape s;
    const QString type = o.value("type").toString();
    for (int i = 0; i < 13; ++i) if (type == kShapeNames[i]) s.type = static_cast<ShapeType>(i);
    for (const auto& v : o.value("points").toArray()) {
        const QJsonArray xy = v.toArray();
        s.points.push_back({xy.at(0).toDouble(), xy.at(1).toDouble()});
    }
    s.color = o.value("color").toString("#d79921").toStdString();
    s.width = o.value("width").toDouble(2.0);
    s.dashed = o.value("dashed").toBool();
    s.flag = o.value("flag").toBool(true);
    s.text = o.value("text").toString().toStdString();
    return s;
}

QJsonObject registerToJson(const LcdRegister& r) {
    auto list = [](const std::vector<std::string>& v) { QJsonArray a; for (const auto& s : v) a.append(QString::fromStdString(s)); return a; };
    return {{"title", QString::fromStdString(r.title)}, {"mode", QString::fromStdString(r.mode)}, {"resultFirst", r.resultFirst},
            {"lines", list(r.lines)}, {"resultLabel", QString::fromStdString(r.resultLabel)}, {"resultValue", QString::fromStdString(r.resultValue)},
            {"extraLines", list(r.extraLines)}, {"footer", QString::fromStdString(r.footer)}, {"isError", r.isError}};
}

LcdRegister registerFromJson(const QJsonObject& o) {
    LcdRegister r;
    r.title = o.value("title").toString().toStdString();
    r.mode = o.value("mode").toString("DEG").toStdString();
    r.resultFirst = o.value("resultFirst").toBool();
    r.lines = strList(o.value("lines"));
    r.resultLabel = o.value("resultLabel").toString().toStdString();
    r.resultValue = o.value("resultValue").toString().toStdString();
    r.extraLines = strList(o.value("extraLines"));
    r.footer = o.value("footer").toString().toStdString();
    r.isError = o.value("isError").toBool();
    return r;
}

QJsonObject snapshotToJson(const Snapshot& s) {
    QJsonArray params, regs;
    for (const auto& [k, v] : s.parameters) params.append(QJsonObject{{"name", QString::fromStdString(k)}, {"value", QString::fromStdString(v)}});
    for (const LcdRegister& r : s.registers) regs.append(registerToJson(r));
    return {{"experiment", QString::fromStdString(s.experiment)}, {"headline", QString::fromStdString(s.headline)},
            {"parameters", params}, {"registers", regs}, {"notesContent", QString::fromStdString(s.notesContent)}};
}

Snapshot snapshotFromJson(const QJsonObject& o) {
    Snapshot s;
    s.experiment = o.value("experiment").toString().toStdString();
    s.headline = o.value("headline").toString().toStdString();
    for (const auto& v : o.value("parameters").toArray()) {
        const QJsonObject p = v.toObject();
        s.parameters.emplace_back(p.value("name").toString().toStdString(), p.value("value").toString().toStdString());
    }
    for (const auto& v : o.value("registers").toArray()) s.registers.push_back(registerFromJson(v.toObject()));
    s.notesContent = o.value("notesContent").toString().toStdString();
    return s;
}

} // namespace

bool JsonSessionStore::save(const SessionLog& log) const {
    QJsonArray runs;
    for (const RunRecord& r : log.runs())
        runs.append(QJsonObject{{"id", r.id}, {"experiment", QString::fromStdString(r.experiment)},
                                {"description", QString::fromStdString(r.description)},
                                {"result", QString::fromStdString(r.result)}, {"valid", r.valid}});
    QJsonArray equations;
    for (const std::string& e : log.equations()) equations.append(QString::fromStdString(e));
    QJsonArray shapes;
    for (const Shape& sh : log.drawing().shapes) shapes.append(shapeToJson(sh));
    QJsonArray snapshots;
    for (const auto& [name, snap] : log.snapshots()) snapshots.append(snapshotToJson(snap));
    QJsonObject recentExport;                                  // the "recent export" card of the Export page
    if (log.hasExport()) {
        const ExportRecord& e = log.lastExport();
        recentExport = {{"path", QString::fromStdString(e.path)}, {"format", QString::fromStdString(e.format)},
                        {"series", QString::fromStdString(e.series)}, {"when", QString::fromStdString(e.when)},
                        {"rows", e.rows}, {"columns", e.columns}};
    }
    const QJsonObject root{{"format", "phy104-session"}, {"version", 4}, {"session", log.sessionNumber()},
                           {"notes", QString::fromStdString(log.notes())}, {"runs", runs}, {"snapshots", snapshots},
                           {"equations", equations}, {"drawing", QJsonObject{{"shapes", shapes}}},
                           {"recentExport", recentExport}};
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QSaveFile file(m_path);                                   // atomic: never leaves a half-written file
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
    std::vector<RunRecord> runs;
    for (const auto& item : root.value("runs").toArray()) {
        const QJsonObject o = item.toObject();
        runs.push_back({o.value("id").toInt(), str(o, "experiment", false), str(o, "description", false),
                        str(o, "result", false), o.value("valid").toBool(true)});
    }
    log.restoreRuns(std::move(runs));
    log.setNotes(root.value("notes").toString().toStdString());
    std::map<std::string, Snapshot> snapshots;
    for (const auto& v : root.value("snapshots").toArray()) {
        Snapshot snap = snapshotFromJson(v.toObject());
        snapshots[snap.experiment] = std::move(snap);
    }
    log.restoreSnapshots(std::move(snapshots));                // re-stamped with the loaded notes
    log.setSessionNumber(root.value("session").toInt(log.sessionNumber()));
    std::vector<std::string> equations;
    for (const auto& e : root.value("equations").toArray()) equations.push_back(e.toString().toStdString());
    log.setEquations(std::move(equations));
    Drawing drawing;
    for (const auto& sh : root.value("drawing").toObject().value("shapes").toArray()) drawing.shapes.push_back(shapeFromJson(sh.toObject()));
    log.setDrawing(std::move(drawing));
    const QJsonObject recent = root.value("recentExport").toObject();      // absent in version 3 files
    if (!recent.isEmpty()) {
        ExportRecord e;
        e.path = str(recent, "path", false);
        e.format = str(recent, "format", false);
        e.series = str(recent, "series", false);
        e.when = str(recent, "when", false);
        e.rows = recent.value("rows").toInt();
        e.columns = recent.value("columns").toInt();
        log.setLastExport(std::move(e));
    }
    return true;
}

std::vector<KeyboardGroup> JsonKeyboardRepository::loadAll() const {
    std::vector<KeyboardGroup> result;
    for (const auto& item : JsonLoader::loadObject(m_path).value("groups").toArray()) {
        const QJsonObject o = item.toObject();
        result.push_back({str(o, "name"), strList(o.value("keys"))});
    }
    return result;
}

} // namespace phy104

namespace phy104 {

std::vector<ScientificPage> JsonKeyboardRepository::scientific() const {
    std::vector<ScientificPage> pages;
    for (const auto& pv : JsonLoader::loadObject(m_path).value("scientific").toArray()) {
        const QJsonObject po = pv.toObject();
        ScientificPage page;
        page.name = po.value("name").toString().toStdString();
        for (const auto& rv : po.value("rows").toArray()) {
            std::vector<ScientificKey> row;
            for (const auto& kv : rv.toArray()) {
                const QJsonObject k = kv.toObject();
                ScientificKey key;
                key.legend = k.value("legend").toString().toStdString();
                key.latex = k.value("latex").toString().toStdString();
                key.shiftLegend = k.value("shiftLegend").toString().toStdString();
                key.shiftLatex = k.value("shiftLatex").toString().toStdString();
                key.control = k.value("control").toString().toStdString();
                row.push_back(std::move(key));
            }
            page.rows.push_back(std::move(row));
        }
        pages.push_back(std::move(page));
    }
    return pages;
}

} // namespace phy104
