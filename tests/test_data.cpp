// Loads the REAL JSON files and checks them:
//  1. every formula parses and every variable is in variables.json
//  2. round trip: for every formula and every variable, solve it back from the others
#include "FormulaEngine.h"
#include "FormulaManager.h"
#include "JsonRepositories.h"
#include "SolverEngine.h"

#include "PhysicsCalculator.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

using namespace phy104;

static std::string full(double v) { std::ostringstream o; o << std::setprecision(15) << v; return o.str(); }

int main() {
    const QString dir = DATA_DIR;
    int failures = 0, solved = 0, skipped = 0;
    try {
        JsonVariableRepository vars(dir + "/variables.json");
        JsonConstantsRepository consts(dir + "/constant.json");
        JsonKeyboardRepository keyboard(dir + "/keyboard.json");
        JsonPresetsRepository presetsRepo(dir + "/optics_presets.json");
        JsonReferenceRepository referenceRepo(dir + "/reference.json");
        JsonStudyHubRepository studyHub(dir + "/study_hub.json");

        FormulaEngine engine(std::make_unique<JsonFormulaRepository>(dir + "/formulas.json"), vars, consts);
        SolverEngine solver(consts.loadAll());

        std::cout << engine.formulas().size() << " formulas, " << vars.loadAll().size() << " variables, "
                  << consts.loadAll().size() << " constants, " << keyboard.loadAll().size()
                  << " keyboard groups, " << referenceRepo.loadAll().size() << " reference topics, " << studyHub.loadAll().size() << " study chapters\n";

        for (const auto& w : engine.warnings()) { std::cerr << "DATA WARNING: " << w << "\n"; ++failures; }

        // ---- the Study Hub: a JSON ARRAY of chapters, in course order, every rule with its explanation ----
        {
            auto expectData = [&](bool ok, const std::string& what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; } };
            const auto topics = studyHub.loadAll();
            expectData(topics.size() >= 8, "study hub has at least 8 chapters");
            expectData(topics.front().id == "refraction" && topics.front().title.find("Refraction") != std::string::npos,
                       "the study hub keeps the order of the JSON array (first chapter)");
            for (const auto& t : topics) {
                expectData(!t.id.empty() && !t.title.empty() && !t.chapter.empty(), "study chapter has id/title/chapter: " + t.title);
                expectData(!t.summary.empty(), "study chapter has a summary: " + t.title);
                expectData(!t.rules.empty(), "study chapter has rules: " + t.title);
                for (const auto& r : t.rules) {
                    expectData(!r.formula.empty(), "rule has a formula: " + t.title);
                    expectData(!r.explain.empty(), "rule is explained: " + t.title);
                }
                for (const auto& c : t.constants) expectData(!c.name.empty() && !c.value.empty(), "constant has name + value: " + t.title);
            }
        }

        // ---- chapters + FormulaManager on the real data ----
        auto expect = [&](bool ok, const std::string& what) { if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; } };
        JsonChapterRepository chapterRepo(dir + "/chapters.json");
        FormulaManager manager(engine, chapterRepo.loadAll());
        const std::vector<std::string> expectedTitles = {
            "Nature of Light & Propagation", "Polarization & Malus's Law", "TIR & Prisms",
            "Spherical Mirrors & Thin Lenses", "Wave Interference", "Thin-Film Interference", "Diffraction"};
        expect(manager.chapters().size() == expectedTitles.size(), "7 chapters");
        for (std::size_t i = 0; i < expectedTitles.size() && i < manager.chapters().size(); ++i)
            expect(manager.chapters()[i].title == expectedTitles[i], "chapter " + expectedTitles[i]);
        const auto counts = manager.countByChapter("");
        int sum = 0;
        for (const auto& c : manager.chapters()) {
            expect(counts.count(c.id) && counts.at(c.id) > 0, "chapter has formulas: " + c.id);
            sum += counts.count(c.id) ? counts.at(c.id) : 0;
        }
        expect(sum == static_cast<int>(engine.formulas().size()), "every formula belongs to a known chapter");
        const auto everything = manager.search("");
        expect(everything.size() == engine.formulas().size(), "empty search returns all formulas");
        expect(everything.front()->chapter == "nature_of_light" && everything.back()->chapter == "diffraction", "chapter sort order");
        expect(manager.search("SNELL").size() == 1 && manager.search("snell_law").size() == 1, "search by name / id");
        expect(manager.search("", "diffraction").size() == 6, "diffraction chapter has 6 formulas");
        expect(FormulaManager::paginate(everything, 0, 5).pageCount == 9, "42 formulas / 5 per page = 9 pages");
        for (const auto& f : engine.formulas()) expect(!f.latex.empty(), "formula has LaTeX: " + f.id);

        // ---- presets used by the experiment pages
        const OpticsPresets presets = presetsRepo.load();
        expect(presets.findMedium("crown") && std::fabs(presets.findMedium("crown")->n - 1.5) < 1e-9, "crown glass n = 1.5000");
        expect(presets.findMedium("air") && presets.findMedium("air")->n == 1.0, "air n = 1");
        for (const auto& m : presets.media)
            if (m.id != "air" && std::fabs(PhysicsCalculator::cauchyIndex(m.cauchyA, m.cauchyB, 589.3) - m.n) > 2e-4)
                expect(false, "Cauchy terms reproduce n at 589.3 nm: " + m.id);
        expect(!presets.sources.empty() && presets.sources[0].wavelengthNm == 532.0, "first light source is the 532 nm green laser");
        expect(referenceRepo.loadAll().size() >= 4 && referenceRepo.unitDiscipline().rfind("Unit discipline", 0) == 0, "reference topics");

        // ---- session file round trip
        {
            const QString path = QDir::tempPath() + "/phy104_test_session.phy";
            JsonSessionStore store(path);
            SessionLog log;
            RunRecord& first = log.addRun("Prisms", "A 60°", "δmin = 37.1808°", true);
            first.series.id = "prism_sweep";
            first.series.name = "Prism sweep";
            first.series.headers = {"d_o (mm)", "d_i (mm)"};
            first.series.rows = {{10, 15.2}, {20, 12.4}, {30, kNaN}};      // a gap survives the round trip
            log.addRun("Wave Interference", "532 nm", "β = 3.1920 mm", false);
            log.setNotes("# Notes\nunicode: θ₂ ≥ θc");
            LcdRegister reg; reg.title = "CALCULATION REGISTER"; reg.lines = {"01  n₁ sin θ₁ = n₂ sin θ₂"}; reg.resultLabel = "θ₂"; reg.resultValue = "48.5904°";
            log.setSnapshot({"Refraction & TIR", {{"n₁", "1.5"}, {"θ₁", "30 °"}}, {reg}, "θ₂ = 48.5904°", {}, ""});
            expect(!log.hasExport(), "a fresh session has no recent export");
            ExportRecord recent;
            recent.path = "/home/student/Documents/PHY104/PHY104_refraction.xlsx";
            recent.format = "Excel (.xlsx)";
            recent.series = "refraction";
            recent.when = "13 Nov 2026 · 01:42";
            recent.rows = 8;
            recent.columns = 3;
            log.setLastExport(recent);
            expect(store.save(log), "session saved");
            QFile savedFile(path);
            const bool savedFileOpened = savedFile.open(QIODevice::ReadOnly);
            expect(savedFileOpened, "saved session can be inspected");
            const QJsonObject savedSession = QJsonDocument::fromJson(savedFile.readAll()).object();
            expect(savedSession.value("version").toInt() == 6, "drawing-free session format is version 6");
            expect(!savedSession.contains("drawing"), "saved session omits removed drawing data");
            SessionLog back;
            expect(store.load(back) && back.runs().size() == 2 && back.runs()[1].id == 2 && !back.runs()[1].valid, "session runs restored");
            expect(back.notes() == log.notes(), "session notes restored (UTF-8)");
            const RunRecord& kept = back.runs()[0];
            expect(kept.series.id == "prism_sweep" && kept.series.headers.size() == 2 && kept.series.rows.size() == 3,
                   "the sweep of a saved run survives the .phy file (version 6)");
            expect(kept.series.rows[2][1] != kept.series.rows[2][1], "a missing cell comes back as NaN, not 0");
            expect(back.runs()[1].series.rows.empty(), "a run without a sweep stays empty");
            const Snapshot* snap = back.snapshot("Refraction & TIR");
            expect(snap && snap->parameters.size() == 2 && snap->parameters[1].second == "30 °", "snapshot parameters restored");
            expect(snap && snap->registers.size() == 1 && snap->registers[0].resultValue == "48.5904°" && snap->registers[0].lines[0] == "01  n₁ sin θ₁ = n₂ sin θ₂",
                   "calculator registers restored");
            expect(snap && snap->notesContent == log.notes(), "snapshot carries the notes after loading");
            expect(back.hasExport() && back.lastExport().path == recent.path && back.lastExport().format == recent.format &&
                   back.lastExport().series == "refraction" && back.lastExport().when == recent.when &&
                   back.lastExport().rows == 8 && back.lastExport().columns == 3, "recent export restored from the .phy file");

            savedFile.close();
            QJsonObject legacySession = savedSession;
            legacySession.insert("version", 5);
            legacySession.insert("drawing", QJsonObject{{"shapes", QJsonArray{QJsonObject{{"type", "pen"}}}}});
            QFile legacyFile(path);
            expect(legacyFile.open(QIODevice::WriteOnly), "legacy session can be written for compatibility check");
            legacyFile.write(QJsonDocument(legacySession).toJson());
            legacyFile.close();
            SessionLog legacyBack;
            expect(store.load(legacyBack) && legacyBack.notes() == log.notes() && legacyBack.runs().size() == log.runs().size(),
                   "legacy session data loads while its drawing payload is ignored");
            QFile::remove(path);
            SessionLog none;
            expect(!JsonSessionStore(QDir::tempPath() + "/does_not_exist.phy").load(none), "missing session file is not an error");
        }

        for (unsigned seed = 1; seed <= 25; ++seed) {   // 25 different random data sets
        std::mt19937 rng(seed);
        auto sample = [&](const std::string& unit) {
            auto u = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
            if (unit == "deg") return u(20, 50);
            if (unit == "rad") return u(0.5, 1.2);
            if (unit.empty())  return u(1.3, 2.5);
            return u(1.0, 5.0);
        };

        for (const auto& formula : engine.formulas()) {
            auto unitOf = [&](const Variable& v) {
                auto it = formula.unitOverrides.find(v.id);
                return it != formula.unitOverrides.end() ? it->second : v.unit;
            };
            std::map<std::string, double> original;
            std::map<std::string, std::string> inputs;
            for (const auto& v : formula.variables) {
                original[v.id] = sample(unitOf(v));
                inputs[v.id] = full(original[v.id]);
            }
            // step 1: compute the first variable from the others
            const std::string first = formula.variables.front().id;
            inputs[first] = "";
            auto base = solver.solve(formula, inputs);
            if (!base.ok) { ++skipped; continue; }             // random numbers not physically valid
            original[first] = base.value;
            inputs[first] = full(base.value);

            // step 2: blank each other variable in turn and recover it
            for (const auto& v : formula.variables) {
                if (v.id == first) continue;
                auto test = inputs;
                test[v.id] = "";
                auto res = solver.solve(formula, test);
                bool good = false;
                if (res.ok) {
                    auto close = [&](double x) {
                        return std::fabs(x - original[v.id]) <= 1e-4 * std::fabs(original[v.id]) + 1e-9; };
                    good = close(res.value);
                    for (double o : res.otherSolutions) good = good || close(o);
                }
                if (good) ++solved;
                else {
                    ++failures;
                    std::cerr << "ROUND TRIP FAIL: " << formula.id << " / " << v.id << " expected "
                              << original[v.id] << " got " << (res.ok ? full(res.value) : res.error) << "\n";
                }
            }
        }
        }
        std::cout << "round trips ok: " << solved << ", formulas skipped (random values invalid): "
                  << skipped << ", failures: " << failures << "\n";
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    return failures == 0 ? 0 : 1;
}
