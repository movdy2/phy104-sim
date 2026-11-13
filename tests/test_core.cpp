// Dependency-free unit tests for the business layer (no Qt, no test framework).
// The repositories are replaced by tiny in-memory fakes: this is why we use interfaces.
#include "ExpressionEvaluator.h"
#include "FormulaEngine.h"
#include "DataExport.h"
#include "EquationText.h"
#include "ExperimentReports.h"
#include "FormulaManager.h"
#include "NumberFormat.h"
#include "SessionLog.h"
#include "WaveExperiments.h"
#include "ReportDocument.h"
#include "XlsxWriter.h"
#include "LatexFormatter.h"
#include "MathCalculator.h"
#include "PhysicsCalculator.h"
#include "SolverEngine.h"
#include "Spectrum.h"
#include "ThickLens.h"

#include <cmath>
#include <iostream>

using namespace phy104;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_failures; \
    std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)
static bool near(double a, double b, double tol = 1e-4) { return std::fabs(a - b) < tol; }

// ---------- fakes ----------
struct FakeFormulas : IFormulaRepository {
    std::vector<Formula> loadAll() const override {
        auto make = [](const char* id, const char* eq) { Formula f; f.id = id; f.name = id; f.equation = eq; f.chapter = "t"; return f; };
        Formula malus = make("malus", "I=Imax*cos(phi)^2");
        malus.unitOverrides["phi"] = "deg";
        return { make("snell", "na*sin(thetaa)=nb*sin(thetab)"), make("crit", "sin(thetac)=nb/na"),
                 make("fringe", "deltaY=lambda*R/d"), make("lens", "1/s+1/sp=1/f"),
                 make("speed", "c=1/sqrt(epsilon0*mu0)"), malus, make("bad", "x=sin(") ,
                 make("ghost", "q=ghostvar*2") };
    }
};
struct FakeVariables : IVariableRepository {
    std::vector<Variable> loadAll() const override {
        auto v = [](const char* id, const char* unit) { Variable x; x.id = id; x.display = id; x.name = id; x.unit = unit; return x; };
        return { v("na",""), v("nb",""), v("thetaa","deg"), v("thetab","deg"), v("thetac","deg"),
                 v("deltaY","m"), v("lambda","m"), v("R","m"), v("d","m"), v("s","m"), v("sp","m"), v("f","m"),
                 v("c","m/s"), v("epsilon0","F/m"), v("mu0","H/m"), v("I","W/m2"), v("Imax","W/m2"), v("phi","rad"), v("q","") };
    }
};
struct FakeConstants : IConstantsRepository {
    std::vector<PhysicsConstant> loadAll() const override {
        return { {"c", "c", "Speed of light", 299792458.0, "m/s", ""},
                 {"epsilon0", "ε₀", "eps", 8.8541878128e-12, "F/m", ""},
                 {"mu0", "μ₀", "mu", 1.25663706212e-6, "H/m", ""} };
    }
};

using Inputs = std::map<std::string, std::string>;

int main() {
    // ---------- evaluator: dynamic input ----------
    CHECK(near(ExpressionEvaluator::evaluate("1+1+1+2-4/24"), 4.833333, 1e-5));   // long chain
    CHECK(near(ExpressionEvaluator::evaluate("2+3*4"), 14));
    CHECK(near(ExpressionEvaluator::evaluate("10-4-2"), 4));                      // left to right
    CHECK(near(ExpressionEvaluator::evaluate("2^3^2"), 512));                     // right to left
    CHECK(near(ExpressionEvaluator::evaluate("-2^2"), -4));
    CHECK(near(ExpressionEvaluator::evaluate("2*(3+4)^2"), 98));
    CHECK(near(ExpressionEvaluator::evaluate("sqrt(abs(-16))+cos(0)"), 5));       // nested functions
    CHECK(near(ExpressionEvaluator::evaluate("sin(pi/6)+cos(pi/3)*sqrt(16)"), 2.5));
    CHECK(near(ExpressionEvaluator::evaluate("asin(sin(0.5)/1)"), 0.5));
    CHECK(near(ExpressionEvaluator::evaluate("3×4÷2"), 6));
    CHECK(near(ExpressionEvaluator::evaluate("√(16)"), 4));
    CHECK(near(ExpressionEvaluator::evaluate("√16+1"), 5));
    CHECK(near(ExpressionEvaluator::evaluate("2*π"), 6.283185, 1e-5));
    CHECK(near(ExpressionEvaluator::evaluate("[1+2]*3"), 9));
    CHECK(near(ExpressionEvaluator::evaluate("3e8"), 3e8, 1));
    CHECK(near(ExpressionEvaluator::evaluate("550*nm"), 5.5e-7, 1e-12));
    CHECK(near(ExpressionEvaluator::evaluate("n*2", {{"n", 1.5}}), 3));
    for (const char* bad : {"2+", "(3", "5/0", "foo(2)", "2 3", "", "x+1"}) {
        bool threw = false;
        try { ExpressionEvaluator::evaluate(bad); } catch (const std::exception&) { threw = true; }
        CHECK(threw);
    }
    CHECK((ExpressionEvaluator::identifiers("na*sin(thetaa)=nb*sin(thetab)+2e3*pi") ==
           std::vector<std::string>{"na", "thetaa", "nb", "thetab"}));

    // ---------- formula engine ----------
    FakeConstants constants;
    FormulaEngine engine(std::make_unique<FakeFormulas>(), FakeVariables{}, constants);
    CHECK(engine.formulas().size() == 8);
    CHECK(engine.findById("snell")->variables.size() == 4);
    CHECK(engine.findById("speed")->findVariable("c") != nullptr);
    CHECK(engine.findById("speed")->findVariable("epsilon0")->defaultValue == "8.8541878128e-12");
    bool badReported = false, ghostReported = false;
    for (const auto& w : engine.warnings()) {
        badReported |= w.find("'bad'") != std::string::npos;
        ghostReported |= w.find("ghostvar") != std::string::npos;
    }
    CHECK(badReported);      // syntax error in an equation is reported
    CHECK(ghostReported);    // variable missing from the dictionary is reported

    // ---------- solver ----------
    SolverEngine solver(constants.loadAll());
    const Formula& snell = *engine.findById("snell");

    auto r = solver.solve(snell, {{"na","1"},{"thetaa","30"},{"nb","1.5"},{"thetab",""}});
    CHECK(r.ok && near(r.value, 19.4712, 1e-3) && r.unit == "deg");
    CHECK(r.steps.size() >= 6);
    auto nb = solver.solve(snell, {{"na","1"},{"thetaa","30"},{"nb",""},{"thetab","19.4712"}});
    CHECK(nb.ok && near(nb.value, 1.5, 1e-3));
    auto na = solver.solve(snell, {{"na",""},{"thetaa","30"},{"nb","1.5"},{"thetab","19.4712"}});
    CHECK(na.ok && near(na.value, 1.0, 1e-3));
    auto ta = solver.solve(snell, {{"na","1"},{"thetaa",""},{"nb","1.5"},{"thetab","19.4712"}});
    CHECK(ta.ok && near(ta.value, 30.0, 1e-3));
    CHECK(!solver.solve(snell, {{"na","1.5"},{"thetaa","60"},{"nb","1"},{"thetab",""}}).ok);   // TIR
    CHECK(solver.solve(snell, {{"na","1"},{"thetaa","30°"},{"nb","1.5"},{"thetab",""}}).ok);   // ° is allowed

    auto crit = solver.solve(*engine.findById("crit"), {{"thetac",""},{"na","1.5"},{"nb","1"}});
    CHECK(crit.ok && near(crit.value, 41.8103, 1e-3));
    CHECK(crit.otherSolutions.empty());                     // 138.19° (= 180° - θc) is not shown

    auto fr = solver.solve(*engine.findById("fringe"),                       // direct substitution + units
                           {{"deltaY",""},{"lambda","550*nm"},{"R","2"},{"d","0.5*mm"}});
    CHECK(fr.ok && near(fr.value, 0.0022, 1e-9));

    const Formula& lens = *engine.findById("lens");
    auto sp = solver.solve(lens, {{"s","30"},{"sp",""},{"f","10"}});
    CHECK(sp.ok && near(sp.value, 15.0, 1e-6));
    auto f = solver.solve(lens, {{"s","30"},{"sp","15"},{"f",""}});
    CHECK(f.ok && near(f.value, 10.0, 1e-6));
    CHECK(!solver.solve(lens, {{"s","10"},{"sp",""},{"f","10"}}).ok);        // a pole is NOT a root

    auto malus = solver.solve(*engine.findById("malus"), {{"I",""},{"Imax","100"},{"phi","60"}});
    CHECK(malus.ok && near(malus.value, 25.0, 1e-6));                        // phi in degrees via override

    // constants typed inside a field
    auto spd = solver.solve(*engine.findById("speed"), {{"c",""},{"epsilon0","ε₀"},{"mu0","μ₀"}});
    CHECK(spd.ok && near(spd.value / 299792458.0, 1.0, 1e-6));

    // wrong number of unknowns / bad text
    CHECK(!solver.solve(snell, {{"na","1"},{"thetaa","30"},{"nb","1.5"},{"thetab","10"}}).ok);
    CHECK(!solver.solve(snell, {{"na",""},{"thetaa",""},{"nb","1.5"},{"thetab","10"}}).ok);
    CHECK(!solver.solve(snell, {{"na","abc"},{"thetaa","30"},{"nb","1.5"},{"thetab",""}}).ok);

    // ---------- physics calculator ----------
    namespace PC = PhysicsCalculator;
    auto ref = PC::refraction(1.0, 1.5, 30);
    CHECK(!ref.totalInternalReflection && near(ref.refractedDeg, 19.4712, 1e-3) && near(ref.reflectedDeg, 30));
    CHECK(PC::refraction(1.5, 1.0, 60).totalInternalReflection);
    CHECK(!PC::refraction(0.0, 1.0, 30).valid);
    CHECK(near(*PC::criticalAngleDeg(1.5, 1.0), 41.8103, 1e-3));
    CHECK(!PC::criticalAngleDeg(1.0, 1.5).has_value());
    CHECK(!PC::refractiveIndex(3e8, 0).has_value());

    auto img = PC::imageFormation(30, 10);
    CHECK(near(img.imageDistance, 15) && near(img.magnification, -0.5) && img.real && !img.upright);
    auto vir = PC::imageFormation(5, 10);
    CHECK(!vir.real && vir.upright && vir.magnification > 1);
    CHECK(PC::imageFormation(10, 10).atInfinity);
    CHECK(!PC::imageFormation(20, 0).valid);
    CHECK(!PC::imageFormation(20, -10).real);

    CHECK(near(PC::fringeSpacing(500e-9, 2.0, 0.5e-3), 2e-3, 1e-9));
    CHECK(near(PC::fringePosition(3, 500e-9, 2.0, 0.5e-3), 6e-3, 1e-9));
    CHECK(near(PC::relativeIntensity(0, 500e-9, 2.0, 0.5e-3), 1.0));
    CHECK(near(PC::relativeIntensity(1e-3, 500e-9, 2.0, 0.5e-3), 0.0, 1e-9));
    CHECK(PC::fringeSpacing(500e-9, 2.0, 0.0) == 0.0);

    // ---------- degree mode ----------
    using D = AngleUnit;
    CHECK(ExpressionEvaluator::evaluate("sin(30)", {}, D::Degrees) > 0.4999999 &&
          ExpressionEvaluator::evaluate("sin(30)", {}, D::Degrees) < 0.5000001);
    CHECK(ExpressionEvaluator::evaluate("sin(180)", {}, D::Degrees) == 0.0);       // exact, not 1.2e-16
    CHECK(ExpressionEvaluator::evaluate("cos(90)", {}, D::Degrees) == 0.0);
    CHECK(ExpressionEvaluator::evaluate("cos(360)", {}, D::Degrees) == 1.0);
    CHECK(near(ExpressionEvaluator::evaluate("asin(0.5)", {}, D::Degrees), 30.0, 1e-9));
    CHECK(near(ExpressionEvaluator::evaluate("atan(1)", {}, D::Degrees), 45.0, 1e-9));
    CHECK(!std::isfinite(ExpressionEvaluator::evaluate("tan(90)", {}, D::Degrees)));

    // ---------- free math calculator ----------
    MathCalculator calc(3);
    CHECK(calc.evaluate("1+1+1+2-4/24").result == "4.833333333");
    CHECK(calc.evaluate("0.1+0.2").result == "0.3");
    CHECK(calc.evaluate("sqrt(abs(-16))+cos(0)").result == "5");
    CHECK(calc.evaluate("2*pi").result == "6.283185307");
    CHECK(calc.evaluate("e").result == "2.718281828");
    CHECK(calc.evaluate("sin(30)").result == "0.5");                               // DEG is the default
    calc.setAngleUnit(AngleUnit::Radians);
    CHECK(calc.evaluate("sin(pi/2)").result == "1");
    CHECK(calc.history().size() == 3);                                             // screen limit = 3
    CHECK(calc.history().front().expression == "e");                               // oldest entries dropped
    auto ansEntry = calc.evaluate("Ans*2");
    CHECK(ansEntry.ok && ansEntry.result == "2");                                  // Ans = sin(pi/2) = 1
    CHECK(!calc.evaluate("1/0").ok && !calc.evaluate("sqrt(-1)").ok && !calc.evaluate("2+").ok);
    CHECK(calc.evaluate("sqrt(-1)").error.find("Math error") != std::string::npos);
    CHECK(near(calc.ans(), 2));                                                    // errors do not change Ans
    calc.setHistoryLimit(1);
    CHECK(calc.history().size() == 1);
    calc.clearHistory();
    CHECK(calc.history().empty() && calc.ans() == 0.0);
    CHECK(MathCalculator::formatResult(-0.0) == "0");
    CHECK(MathCalculator::formatResult(1e20) == "1e+20");

    // ---------- LaTeX formatter ----------
    CHECK(LatexFormatter::number(19.4712) == "19.4712");
    CHECK(LatexFormatter::number(5.5e-7) == "5.5\\times10^{-7}");
    CHECK(LatexFormatter::number(2.99792458e8) == "2.99792\\times10^{8}");
    CHECK(LatexFormatter::unit("deg") == "^{\\circ}");
    CHECK(LatexFormatter::answer("\\theta_{b}", 19.4712, "deg") == "\\theta_{b}=19.4712^{\\circ}");
    CHECK(LatexFormatter::answer("\\Delta y", 0.0022, "m") == "\\Delta y=0.0022\\,\\text{m}");

    // ---------- formula manager: search, chapters, paging ----------
    struct F2 : IFormulaRepository {
        std::vector<Formula> loadAll() const override {
            auto mk = [](const char* id, const char* name, const char* ch) {
                Formula f; f.id = id; f.name = name; f.equation = "a=b"; f.chapter = ch; return f; };
            return { mk("thin_lens", "Thin lens equation", "lenses"), mk("snell_law", "Snell's Law", "light"),
                     mk("malus_law", "Malus Law", "pol"), mk("critical_angle", "Critical angle", "tir"),
                     mk("mirror_equation", "Mirror equation", "lenses"), mk("zzz", "Mystery", "unknown_chapter"),
                     mk("refractive_index", "Refractive index", "light") };
        }
    };
    struct V2 : IVariableRepository { std::vector<Variable> loadAll() const override { return { Variable{"a","a","a","","","",""}, Variable{"b","b","b","","","",""} }; } };
    FormulaEngine engine2(std::make_unique<F2>(), V2{}, constants);
    FormulaManager manager(engine2, {{"tir", "TIR", 3}, {"light", "Light", 1}, {"pol", "Polarization", 2}, {"lenses", "Lenses", 4}});

    CHECK(manager.chapters().front().id == "light" && manager.chapters().back().id == "lenses");   // sorted by order
    auto all = manager.search("");
    CHECK(all.size() == 7);
    CHECK(all[0]->chapter == "light" && all[1]->chapter == "light");               // chapter order first ...
    CHECK(all[0]->id == "refractive_index" && all[1]->id == "snell_law");          // ... then by name
    CHECK(all.back()->id == "zzz");                                                // unknown chapter last
    CHECK(manager.search("LENS").size() == 1);                                     // case-insensitive, name
    CHECK(manager.search("snell").size() == 1 && manager.search("SNELL_LAW").size() == 1);   // id, underscores
    CHECK(manager.search("critical angle").size() == 1 && manager.search("angle critical").size() == 1);
    CHECK(manager.search("equation").size() == 2);
    CHECK(manager.search("nothing here").empty());
    CHECK(manager.search("", "lenses").size() == 2 && manager.search("mirror", "light").empty());
    CHECK(manager.countByChapter("").at("light") == 2);
    CHECK(manager.chapterTitle("tir") == "TIR" && manager.chapterTitle("zzz") == "zzz");

    auto p0 = FormulaManager::paginate(all, 0, 3);
    CHECK(p0.items.size() == 3 && p0.pageCount == 3 && p0.pageIndex == 0 && p0.total == 7);
    auto p2 = FormulaManager::paginate(all, 2, 3);
    CHECK(p2.items.size() == 1);                                                   // last page is shorter
    CHECK(FormulaManager::paginate(all, 99, 3).pageIndex == 2);                    // clamped
    CHECK(FormulaManager::paginate(all, -4, 3).pageIndex == 0);
    CHECK(FormulaManager::paginate({}, 0, 5).pageCount == 1);                      // empty result
    CHECK(FormulaManager::paginate(all, 0, 0).items.size() == 1);                  // page size >= 1

    // ====================================================================== Figma design numbers
    namespace ER = ExperimentReports;
    auto hasLine = [](const LcdRegister& r, const std::string& text) {
        for (const auto& l : r.lines) if (l == text) return true;
        for (const auto& l : r.extraLines) if (l == text) return true;
        return false;
    };
    // --- number formatting
    CHECK(NumberFormat::fixed(48.59037789) == "48.5904" && NumberFormat::fixed(-0.00000001) == "0.0000");
    CHECK(NumberFormat::signedFixed(15) == "+15.0000" && NumberFormat::signedFixed(-0.5) == "-0.5000");
    CHECK(NumberFormat::scientific(5.32e-7) == "5.320 × 10⁻⁷" && NumberFormat::scientific(2.5e-4) == "2.500 × 10⁻⁴");
    CHECK(NumberFormat::scientific(9.99996e-5) == "1.000 × 10⁻⁴");
    CHECK(NumberFormat::general(0.5) == "0.5" && NumberFormat::general(30) == "30");

    // --- Refraction & TIR (glass crown -> air, 30°)
    {
        auto r = ER::refraction({});
        CHECK(r.valid && !r.paused && r.registers.size() == 2);
        CHECK(r.registers[0].resultValue == "48.5904°" && r.registers[0].resultLabel == "REFRACTION ANGLE · θ₂");
        CHECK(hasLine(r.registers[0], "02  sin θ₂ = 1.5 × 0.5") && hasLine(r.registers[0], "03  sin θ₂ = 0.7500"));
        CHECK(hasLine(r.registers[0], "04  θ₂ = asin(0.7500)"));
        CHECK(r.registers[1].title == "TIR COMPARISON" && hasLine(r.registers[1], "θc = 41.8103°"));
        CHECK(hasLine(r.registers[1], "θ₁ = 50° > θc") && hasLine(r.registers[1], "sin θ₂ = 1.1491 > 1"));
        CHECK(hasLine(r.registers[1], "STATUS: TOTAL REFLECTION"));
        CHECK(r.headline == "θ₂ = 48.5904°" && r.notices[0].title == "Physical limit, not a solver error");
        RefractionInput tir; tir.theta1 = 60;                                   // case A itself is TIR
        CHECK(ER::refraction(tir).registers[0].resultValue == "TIR");
        RefractionInput up; up.n1 = 1.0; up.n2 = 1.5;                           // air -> glass: no critical angle
        CHECK(hasLine(ER::refraction(up).registers[1], "STATUS: REFRACTION ONLY"));
        CHECK(std::fabs(ER::defaultTirAngle(1.5, 1.0) - 50.0) < 1e-9);
        auto sweep = ER::refractionSweep({});
        CHECK(sweep.rows.size() == 8 && std::fabs(sweep.rows[6][1] - 48.5904) < 1e-3);
        CHECK(sweep.description == "Crown glass → Air · n₁ = 1.5000 · n₂ = 1.0000" && sweep.note == "sin θ is dimensionless");
        std::vector<double> xs, ys;
        for (auto& row : sweep.rows) if (!std::isnan(row[1])) { xs.push_back(row[2]); ys.push_back(row[3]); }
        auto fit = ER::linearFit(xs, ys);
        CHECK(fit.ok && std::fabs(fit.slope - 1.5) < 1e-9 && std::fabs(fit.r2 - 1.0) < 1e-9);
    }
    // --- the new TIR screen (Figma: Refraction & TIR / Total internal reflection)
    {
        RefractionInput in; in.tirMode = true; in.theta1 = 50;
        auto r = ER::refraction(in);
        CHECK(r.valid && r.registers.size() == 2 && r.registers[0].title == "CRITICAL ANGLE REGISTER" && r.registers[0].mode == "OK");
        CHECK(hasLine(r.registers[0], "01 θc = asin(n₂ / n₁)") && hasLine(r.registers[0], "02 θc = asin(1.0 / 1.5)") && hasLine(r.registers[0], "03 θc = 41.8103°"));
        CHECK(hasLine(r.registers[0], "04 θ₁ = 50.0000° > θc") && hasLine(r.registers[0], "05 1.5 sin(50°) = 1.1491") && hasLine(r.registers[0], "06 1.1491 > 1 → no real θ₂"));
        CHECK(r.registers[0].resultLabel == "PHYSICAL OUTCOME" && r.registers[0].resultValue == "TIR");
        CHECK(r.registers[1].title == "REFLECTION REGISTER" && hasLine(r.registers[1], "θᵣ = θ₁ = 50.0000°"));
        CHECK(hasLine(r.registers[1], "TRANSMISSION: NONE") && hasLine(r.registers[1], "STATUS: VALID / TIR"));
        CHECK(r.registers[1].resultLabel == "ABOVE CRITICAL BY" && r.registers[1].resultValue == "8.1897°");
        CHECK(r.notices[0].title == "Valid physical outcome, not an error" && !r.notices[0].error && r.statusText == "Valid outcome: TIR");
        in.theta1 = 30;                                                          // below the critical angle: refraction, still valid
        auto b = ER::refraction(in);
        CHECK(b.registers[0].resultValue == "PARTIAL" && b.registers[1].resultLabel == "BELOW CRITICAL BY" && b.registers[1].resultValue == "11.8103°");
        auto sw = ER::tirSweep({});
        CHECK(sw.rows.size() == 19 && !std::isnan(sw.rows[8][1]) && std::isnan(sw.rows[9][1]));   // 40° ok, 45° is TIR
    }
    // --- Lenses & Mirrors: thin lens (f = 10, do = 30, ho = 3)
    {
        auto r = ER::imaging({});
        CHECK(r.valid && hasLine(r.registers[0], "02  1/dᵢ = 1/10 - 1/30") && hasLine(r.registers[0], "03  1/dᵢ = 0.066667 cm⁻¹"));
        CHECK(hasLine(r.registers[0], "04  dᵢ = +15.0000 cm") && r.registers[0].resultValue == "-0.5000");
        CHECK(r.registers[1].title == "IMAGE PROPERTIES" && hasLine(r.registers[1], "hᵢ = -1.5000 cm"));
        CHECK(hasLine(r.registers[1], "IMAGE: REAL") && hasLine(r.registers[1], "ORIENTATION: INVERTED") && hasLine(r.registers[1], "MIRROR: SAME RESULTS"));
        CHECK(r.notices[0].error && r.notices[0].title == "Guarded case / E_FOCAL_PLANE" && r.headline == "dᵢ = +15.0000 cm");
        CHECK(r.retained.resultValue == "+15.0000 cm" && hasLine(r.retained, "m = -0.5000 · hᵢ = -1.5000 cm"));
        ImagingInput div; div.f = -10;
        CHECK(hasLine(ER::imaging(div).registers[1], "IMAGE: VIRTUAL") && hasLine(ER::imaging(div).registers[1], "ORIENTATION: UPRIGHT"));
        CHECK(ER::imagingSweep({}).rows.size() == 8);
    }
    // --- the new convex mirror screen (f = -10, do = +30, ho = +3)
    {
        ImagingInput in; in.f = -10; in.mirrorMode = true;
        auto r = ER::imaging(in);
        CHECK(r.valid && r.registers[0].title == "GAUSSIAN MIRROR REGISTER" && r.registers[0].mode == "OK");
        CHECK(hasLine(r.registers[0], "01  1/f = 1/dₒ + 1/dᵢ") && hasLine(r.registers[0], "02  1/dᵢ = 1/(-10) - 1/30"));
        CHECK(hasLine(r.registers[0], "03  1/dᵢ = -0.133333 cm⁻¹") && hasLine(r.registers[0], "04  dᵢ = (-0.133333)⁻¹"));
        CHECK(r.registers[0].resultLabel == "IMAGE DISTANCE / cm" && r.registers[0].resultValue == "-7.5000");
        CHECK(hasLine(r.registers[1], "m = -dᵢ/dₒ = +0.2500") && hasLine(r.registers[1], "hᵢ = m × hₒ = +0.7500 cm"));
        CHECK(hasLine(r.registers[1], "IMAGE: VIRTUAL") && hasLine(r.registers[1], "ORIENTATION: UPRIGHT") && hasLine(r.registers[1], "SIZE: DIMINISHED (¼×)"));
        CHECK(r.notices[0].title == "Negative distance ≠ invalid input" && r.footnote == "P: pole · F: -10 cm · C: -20 cm");
        CHECK(r.statusText == "Virtual image · negative distances are valid");
        CHECK(ER::sizeDescription(2.5) == "ENLARGED (2.5×)" && ER::sizeDescription(-1.0) == "SAME SIZE (1×)" && ER::sizeDescription(0.5) == "DIMINISHED (½×)");
        in.f = 10;                                                               // concave: real image in front
        CHECK(ER::imaging(in).notices[0].title == "Real image in front of the mirror");
    }
    // --- Prisms (n = 1.5, A = 60°, symmetric passage)
    {
        auto r = ER::prism({});
        CHECK(r.valid && r.registers[0].resultValue == "37.1808°" && r.registers[0].resultLabel == "MINIMUM DEVIATION · δmin");
        CHECK(hasLine(r.registers[0], "    r₁ = 30.0000°") && hasLine(r.registers[0], "    i₂ = 48.5904°"));
        CHECK(r.notices[0].title == "Symmetry verified" && r.notices.back().title == "Exit-face validation");
        CHECK(r.footnote.find("1.5000") != std::string::npos && r.headline == "δmin = 37.1808°");
        PrismInput off; off.incidence = 40;
        CHECK(ER::prism(off).registers[0].resultLabel == "DEVIATION · δ" && ER::prism(off).notices[0].title == "Off the minimum");
        PrismInput tir; tir.nPrism = 2.4; tir.incidence = 5;                      // TIR at the exit face: a valid physical outcome
        auto t = ER::prism(tir);
        CHECK(t.valid && !t.paused && t.errorCode == "E_EXIT_TIR" && t.registers[0].resultValue == "TIR");
        CHECK(std::fabs(PhysicsCalculator::minimumDeviationDeg(1.5, 1.0, 60).value() - 37.1808) < 1e-3);
        CHECK(std::fabs(PhysicsCalculator::indexFromMinimumDeviation(37.1808, 60).value() - 1.5) < 1e-4);
        CHECK(std::fabs(PhysicsCalculator::cauchyIndex(1.4879057, 0.0042, 589.3) - 1.5) < 1e-5);
        CHECK(ER::prismSweep({}).rows.size() == 7);
    }
    // --- Double slit (532 nm, d = 0.25 mm, L = 1.5 m, m = 2)
    SlitInput slit;
    auto good = ER::doubleSlit(slit);
    {
        CHECK(good.valid && hasLine(good.registers[0], "01  λ = 5.320 × 10⁻⁷ m") && hasLine(good.registers[0], "02  d = 2.500 × 10⁻⁴ m"));
        CHECK(hasLine(good.registers[0], "    β = 0.003192 m") && good.registers[0].resultValue == "3.1920 mm");
        CHECK(good.registers[1].title == "SECOND BRIGHT FRINGE" && hasLine(good.registers[1], "y₂ = +6.3840 mm"));
        CHECK(hasLine(good.registers[1], "Δ = 2λ = 1064.0 nm") && hasLine(good.registers[1], "I(y₂) / I₀ = 1.0000") && good.headline == "β = 3.1920 mm");
    }
    // --- the new invalid-input pattern (Figma: Wave Interference / Invalid input)
    {
        SlitInput bad; bad.separationMm = 0;
        ReportContext ctx{&good, "RUN 021"};
        auto e = ER::doubleSlit(bad, ctx);
        CHECK(!e.valid && e.paused && e.errorCode == "E_INPUT_RANGE" && e.registers.size() == 2);
        CHECK(e.registers[0].title == "CURRENT INPUT / PAUSED" && e.registers[0].resultFirst && e.registers[0].resultValue == "E_INPUT_RANGE");
        CHECK(hasLine(e.registers[0], "d = 0.000 mm") && hasLine(e.registers[0], "β = λL / d → UNDEFINED") && hasLine(e.registers[0], "ACTION: ENTER d > 0"));
        CHECK(e.registers[0].footer == "NO NEW RESULT / MEMORY RETAINED");
        CHECK(e.registers[1].title == "LAST VALID / RUN 021" && hasLine(e.registers[1], "λ = 532.0 nm") && hasLine(e.registers[1], "d = 0.250 mm · L = 1.500 m"));
        CHECK(e.registers[1].resultLabel == "RETAINED FRINGE SPACING · β" && e.registers[1].resultValue == "3.1920 mm");
        CHECK(hasLine(e.registers[1], "m = 2 · y₂ = +6.3840 mm") && e.registers[1].footer == "STALE / NOT CURRENT INPUTS");
        CHECK(e.issues.size() == 1 && e.issues[0].field == "d" && e.issues[0].message == "Enter d > 0 mm." && e.issues[0].label == "d");
        CHECK(e.notices[0].error && e.notices[0].title == "Calculation paused / d must be > 0");
        CHECK(e.notices[0].body.find("run 021, not the current inputs") != std::string::npos);
        CHECK(e.statusText == "E_INPUT_RANGE · last valid run 021 retained" && e.headline == "β = 3.1920 mm");
        auto none = ER::doubleSlit(bad);                                          // no previous result to retain
        CHECK(none.paused && none.registers.size() == 1 && none.notices[0].body.find("the last valid result") != std::string::npos);
        RefractionInput rb; rb.theta1 = 95;
        auto re = ER::refraction(rb);
        CHECK(re.paused && re.issues[0].field == "theta1" && re.issues[0].message == "Enter 0° ≤ θ₁ < 90°.");
        ImagingInput ib; ib.s = 10;                                               // focal plane
        auto ie = ER::imaging(ib);
        CHECK(ie.paused && ie.errorCode == "E_FOCAL_PLANE" && ie.issues[0].field == "s" && hasLine(ie.registers[0], "dₒ = f = 10 cm"));
        ImagingInput ib0; ib0.f = 0;
        CHECK(ER::imaging(ib0).issues[0].field == "f");
        CHECK(ER::doubleSlitSweep(bad).rows[0][1] != ER::doubleSlitSweep(bad).rows[0][1]);   // NaN: nothing to plot
    }
    // --- session log
    {
        SessionLog log;
        CHECK(log.addRun("Prisms", "Single ray", "δmin = 37.1808°", true).id == 1);
        CHECK(log.addRun("Wave Interference", "532 nm", "β = 3.1920 mm", true).id == 2);
        CHECK(log.savedCalculations() == 2 && log.recent(1).front().id == 2 && log.recent(5).size() == 2);
        CHECK(SessionLog::runLabel(14) == "#014");
        CHECK(log.snapshot("Prisms") == nullptr);
        log.setSnapshot({"Prisms", {{"A", "60 °"}}, {}, "δ", {}, ""});
        CHECK(log.snapshot("Prisms")->parameters.size() == 1);
        // the dashboard's "delete selected run"
        log.addRun("Prisms", "Second sweep", "n = 1.5", true);
        CHECK(log.runs().size() == 3 && log.removeRun(2));
        CHECK(log.runs().size() == 2 && log.runs()[0].id == 1 && log.runs()[1].id == 3);
        CHECK(!log.removeRun(2) && !log.removeRun(9999) && log.runs().size() == 2);   // unknown ids are refused
    }
    // --- notes travel with the snapshots
    {
        SessionLog log;
        log.setSnapshot({"Prisms", {{"A", "60 °"}}, {}, "δ", {}, "ignored"});
        CHECK(log.snapshot("Prisms")->notesContent.empty());                 // no notes yet: the field of the argument is overwritten
        log.setNotes("# Optics\n- $n₁ sin θ₁$");
        CHECK(log.snapshot("Prisms")->notesContent == "# Optics\n- $n₁ sin θ₁$");   // existing snapshots follow the notes
        log.setSnapshot({"Wave Interference", {}, {}, "β", {}, ""});
        CHECK(log.snapshot("Wave Interference")->notesContent == log.notes());        // new snapshots are stamped with the current notes
        log.setNotes("changed");
        CHECK(log.snapshot("Prisms")->notesContent == "changed" && log.snapshot("Wave Interference")->notesContent == "changed");
        std::map<std::string, Snapshot> restored;
        restored["Prisms"] = {"Prisms", {}, {}, "δ", {}, "stale"};
        log.restoreSnapshots(restored);
        CHECK(log.snapshot("Prisms")->notesContent == "changed");                      // loaded snapshots are re-stamped
    }
    // --- export: CRC, CSV, workbook
    {
        const std::string check = "123456789";
        CHECK(XlsxWriter::crc32(reinterpret_cast<const std::uint8_t*>(check.data()), check.size()) == 0xCBF43926u);
        CHECK(XlsxWriter::columnName(0) == "A" && XlsxWriter::columnName(25) == "Z" && XlsxWriter::columnName(27) == "AB");
        CHECK(XlsxWriter::xmlEscape("a<b&\"c\">") == "a&lt;b&amp;&quot;c&quot;&gt;");
        auto series = ER::refractionSweep({});
        const std::string csv = DataExport::toCsv(series);
        CHECK(csv.rfind("θ₁ (°),θ₂ (°),sin θ₁,sin θ₂\n", 0) == 0);
        RefractionInput dense; dense.n1 = 2.4;                                    // critical angle 24.6°: rows from 25° are TIR
        const std::string csvTir = DataExport::toCsv(ER::refractionSweep(dense));
        CHECK(csvTir.find("30,,0.5,1.2\n") != std::string::npos);                // TIR row: empty cell, not "nan"
        CHECK(csvTir.find("nan") == std::string::npos);
        SessionLog log; log.setNotes("line one\nline two");
        log.setSnapshot({"Refraction & TIR", {{"n₁", "1.5"}}, {ER::refraction({}).registers[0]}, "x", {}, ""});
        auto sheets = DataExport::workbookSheets(log, series, {});
        CHECK(sheets.size() == 4 && sheets[0].name == "Parameters" && sheets[3].name == "Notes");
        ExportOptions only; only.parameters = only.steps = only.notes = false;
        CHECK(DataExport::workbookSheets(log, series, only).size() == 1);
        const auto bytes = DataExport::buildWorkbook(log, series, {});
        CHECK(bytes.size() > 500 && bytes[0] == 'P' && bytes[1] == 'K');
        CHECK(DataExport::describeWorkbook(log, series, {}).rfind("4 sheets · 8 measurements · ", 0) == 0);
        CHECK(DataExport::suggestedFileName(series, ".xlsx") == "PHY104_refraction.xlsx");
        if (FILE* f = std::fopen("/tmp/phy104_test.xlsx", "wb")) { std::fwrite(bytes.data(), 1, bytes.size(), f); std::fclose(f); }
    }

    // ====================================================================== equation text (Equation Lab)
    {
        auto toExpr = [](const char* s) { return EquationText::latexToExpression(s); };
        CHECK(toExpr("n_1 \\sin\\theta_1 = n_2 \\sin\\theta_2") == "n_1*sin(theta_1)=n_2*sin(theta_2)");
        CHECK(toExpr("\\frac{1}{s}+\\frac{1}{s'}=\\frac{1}{f}") == "((1)/(s))+((1)/(sp))=((1)/(f))");
        CHECK(toExpr("I = I_{max}\\cos^2\\phi") == "I=I_max*(cos(phi))^(2)");
        CHECK(toExpr("c=\\frac{1}{\\sqrt{\\epsilon_0\\mu_0}}") == "c=((1)/(sqrt(epsilon_0*mu_0)))");
        // the converted text must be evaluable and give the right number
        auto eval = [](const std::string& text, VariableMap v) { return ExpressionEvaluator::evaluate(text, v); };
        CHECK(near(eval(toExpr("\\frac{a}{b}+\\sqrt{16}"), {{"a", 3}, {"b", 4}}), 4.75));
        CHECK(near(eval(toExpr("x^{2}+3x"), {{"x", 2}}), 10));
        CHECK(near(eval(toExpr("\\sin\\left(\\frac{\\pi}{2}\\right)"), {}), 1));
        for (const char* bad : {"", "\\unknown{x}", "\\frac{1}", "a_", "}"}) {
            bool threw = false;
            try { toExpr(bad); } catch (const std::exception&) { threw = true; }
            CHECK(threw);
        }
        CHECK(EquationText::expressionToLatex("n1*sin(theta1)=n2*sin(theta2)") == "n_{1} \\sin\\left(\\theta_{1}\\right) = n_{2} \\sin\\left(\\theta_{2}\\right)");
        CHECK(EquationText::expressionToLatex("1/s+1/sp=1/f") == "\\frac{1}{s} + \\frac{1}{s_{p}} = \\frac{1}{f}");
        CHECK(EquationText::expressionToLatex("sqrt(x^2+y^2)") == "\\sqrt{x^{2} + y^{2}}");
        CHECK(EquationText::expressionToLatex("a*b/c+2*3") == "\\frac{a b}{c} + 2 \\times 3");
        bool threw = false;
        try { EquationText::expressionToLatex("2+"); } catch (const std::exception&) { threw = true; }
        CHECK(threw);
    }
    // ====================================================================== the six new Figma screens
    {
        namespace WE = WaveExperiments;
        auto has = [](const LcdRegister& r, const std::string& text) { for (const auto& l : r.lines) if (l == text) return true; return false; };
        // --- Newton's rings: λ = 600 nm, R = 1 m, m = 4, n = 1
        auto n = WE::newtonRings({});
        const auto& nr = n.report;
        CHECK(nr.valid && nr.registers[0].title == "REFLECTED DARK RING" && has(nr.registers[0], "rₘ² = mλR / n"));
        CHECK(has(nr.registers[0], " = 4 × 600×10⁻⁹ × 1 / 1") && has(nr.registers[0], " = 2.4000 × 10⁻⁶ m²") && has(nr.registers[0], "r₄ = √(2.4×10⁻⁶) m"));
        CHECK(nr.registers[0].resultLabel == "RADIUS · r₄" && nr.registers[0].resultValue == "1.5492 mm");
        CHECK(nr.registers[1].title == "DIAMETER / FILM THICKNESS" && has(nr.registers[1], "D₄ = 2r₄") && has(nr.registers[1], "t₄ = mλ/(2n) = 1200 nm"));
        CHECK(has(nr.registers[1], "m = 0 → r₀ = 0 (dark)") && nr.registers[1].resultValue == "3.0984 mm");
        CHECK(nr.notices[0].title == "Interpretation" && nr.notices[1].title == "Input guard");
        CHECK(near(n.chart.curve.back().second / n.chart.curve.back().first, 0.6, 1e-9) && n.chart.legend.rfind("Slope = λR/n = 0.6000 mm²/order", 0) == 0);
        CHECK(n.chart.marker.size() == 1 && near(n.chart.marker[0].second, 2.4, 1e-9));
        NewtonInput nb; nb.order = 2.5;
        auto ne = WE::newtonRings(nb);
        CHECK(ne.report.paused && ne.report.issues[0].field == "m" && ne.report.issues[0].message == "Enter an integer m ≥ 0.");
        // --- thin film: λ = 600 nm, n = 1.5, t = 100 nm
        auto f = WE::thinFilm({});
        CHECK(has(f.report.registers[0], "OPD = 2nt") && has(f.report.registers[0], " = 2 × 1.5 × 100 nm") && has(f.report.registers[0], " = 300 nm"));
        CHECK(has(f.report.registers[0], "Δψ = 4πnt/λ − π = 0.0000 rad") && f.report.registers[0].resultValue == "1.0000");
        CHECK(f.report.registers[1].title == "ONE PHASE REVERSAL" && f.report.registers[1].resultValue == "100.0000 nm" && has(f.report.registers[1], "Dark: 2nt = qλ"));
        ThinFilmInput dark; dark.thicknessNm = 200;
        CHECK(near(std::stod(WE::thinFilm(dark).report.registers[0].resultValue), 0.0, 1e-9));              // t = λ/(2n): dark
        // --- diffraction grating: N = 5000 lines/cm, λ = 600 nm, m = +1, M = 20
        auto g = WE::grating({});
        CHECK(has(g.report.registers[0], "d = 0.01 / N") && has(g.report.registers[0], " = 0.01 / 5000 m") && has(g.report.registers[0], " = 2.0000 × 10⁻⁶ m"));
        CHECK(has(g.report.registers[0], "sin θ₁ = 600 nm / 2000 nm") && g.report.registers[0].resultLabel == "ORDER +1 ANGLE" && g.report.registers[0].resultValue == "+17.4576°");
        CHECK(has(g.report.registers[1], "|m| ≤ floor(d/λ) = 3") && has(g.report.registers[1], "m = -3, -2, -1, 0, 1, 2, 3") && g.report.registers[1].resultValue == "7 orders");
        CHECK(g.report.notices[0].body.find("±2 at ±36.87°") != std::string::npos && g.report.notices[0].body.find("±3 at ±64.16°") != std::string::npos);
        CHECK(WE::allowedGratingOrders(5000, 600, 0).size() == 7 && near(WE::gratingIntensity(0, 5000, 600, 20), 1.0) && WE::gratingIntensity(5, 5000, 600, 20) < 0.1);
        CHECK(near(WE::gratingIntensity(17.4576, 5000, 600, 20), 1.0, 1e-3));                              // the +1 order is a principal maximum
        GratingInput g4; g4.order = 4;
        auto ge = WE::grating(g4);
        CHECK(ge.report.paused && ge.report.issues[0].field == "m" && ge.report.issues[0].message == "No real angle: |mλ/d| > 1.");
        GratingInput g0; g0.order = 0;
        CHECK(WE::grating(g0).report.registers[0].resultValue == "0.0000°");
        // --- single slit: a = 0.1 mm, λ = 500 nm, L = 1 m
        auto s = WE::singleSlit({});
        CHECK(s.report.registers[0].title == "FIRST MINIMUM / DEG" && has(s.report.registers[0], "a sin θ₁ = λ") && has(s.report.registers[0], "sin θ₁ = 5×10⁻⁷ / 10⁻⁴"));
        CHECK(has(s.report.registers[0], "θ₁ = asin(0.005)") && s.report.registers[0].resultValue == "0.2865°");
        CHECK(has(s.report.registers[1], "w = 2L tan θ₁") && has(s.report.registers[1], " = 2 × 1 × tan(0.2865°)") && has(s.report.registers[1], " = 0.010000125 m"));
        CHECK(s.report.registers[1].resultValue == "10.0001 mm" && s.report.notices[0].body.find("±5.0001 mm") != std::string::npos);
        CHECK(near(WE::singleSlitIntensity(0, 0.1, 500), 1.0) && near(WE::singleSlitIntensity(0.28648, 0.1, 500), 0.0, 1e-6));
        CHECK(near(s.chart.xMax, 1.2, 1e-9));
        SingleSlitInput sb; sb.widthMm = 0.0001;                                                           // λ/a > 1: no first minimum
        CHECK(WE::singleSlit(sb).report.paused);
        // --- Malus + Brewster: Iin = 100 W/m² unpolarized, θ = 60°, air -> glass
        auto p = WE::polarization({});
        CHECK(has(p.report.registers[0], "I₀ = Iᵢₙ / 2 = 50 W/m²") && has(p.report.registers[0], "I = I₀ cos²θ") && has(p.report.registers[0], "  = 50 × cos²(60°)") && has(p.report.registers[0], "  = 50 × 0.2500"));
        CHECK(p.report.registers[0].resultLabel == "ANALYZER OUTPUT" && p.report.registers[0].resultValue == "12.5000 W/m²");
        CHECK(p.report.registers[1].title == "BREWSTER / AIR → GLASS" && has(p.report.registers[1], "θB = atan(1.5 / 1.0)") && p.report.registers[1].resultValue == "56.3099°");
        CHECK(p.report.notices[0].body.find("passes 25% of I₀, or 12.5% of the original beam") != std::string::npos);
        PolarizationInput pp; pp.unpolarized = false;
        CHECK(WE::polarization(pp).report.registers[0].resultValue == "25.0000 W/m²");
        // --- Michelson (values derived from the physics; the frame was not read number by number)
        MichelsonInput mi; mi.displacementUm = 0.31640;                                                    // ≈ λ/(2n): one fringe
        auto mm = WE::michelson(mi);
        CHECK(near(std::stod(mm.report.registers[0].resultValue), 1.0, 1e-3) && near(std::stod(mm.report.registers[1].resultValue), 1.0, 1e-3));
        MichelsonInput half; half.displacementUm = 0.1582;                                                 // half a fringe: dark
        CHECK(near(std::stod(WE::michelson(half).report.registers[1].resultValue), 0.0, 1e-3));
        MichelsonInput badm; badm.n = 0.5;
        CHECK(WE::michelson(badm).report.paused);
        // --- second mode tabs: Newton transmitted, Michelson fringe count, thin-film transmission
        NewtonInput nt; nt.transmitted = true;
        auto ntr = WE::newtonRings(nt);
        CHECK(ntr.report.registers[0].title == "TRANSMITTED DARK RING" && has(ntr.report.registers[0], "rₘ² = (m + ½)λR / n"));
        CHECK(ntr.report.registers[0].resultValue == "1.6432 mm" && ntr.report.registers[1].resultValue == "3.2863 mm");
        CHECK(near(ntr.chart.marker[0].second, 2.7, 1e-9));                                                // r² at the m + ½ order
        CHECK(ntr.report.registers[0].resultValue != nr.registers[0].resultValue);
        MichelsonInput mf; mf.fringes = true;
        auto fm = WE::michelson(mf);
        CHECK(fm.report.registers[0].title == "FRINGE SPACING" && fm.report.registers[0].resultValue == "316.4000 nm");
        CHECK(fm.report.registers[1].resultValue == "12.6422 fringes" && fm.chart.curve.size() == 601);
        CHECK(near(fm.chart.xMin, -2.0) && near(fm.chart.xMax, 2.0) && near(fm.chart.marker[0].second, 0.1568, 1e-3));
        MichelsonInput oneFringe; oneFringe.fringes = true; oneFringe.displacementUm = 0.3164;             // exactly λ/(2n): back to the same intensity
        CHECK(near(WE::michelson(oneFringe).chart.marker[0].second, 1.0, 1e-3));
        MichelsonInput piez; piez.fringes = true; piez.piezo = true;
        bool piezoScan = false;
        for (const auto& [key, value] : WE::michelson(piez).report.parameters) if (key == "mount" && value == "piezo scan") piezoScan = true;
        CHECK(piezoScan);
        CHECK(fm.report.parameters.back().second == "fringes" && mm.report.parameters.back().second == "displacement");
        ThinFilmInput tf; tf.transmission = true;
        auto ft = WE::thinFilm(tf);
        CHECK(ft.report.registers[0].resultLabel == "NORMALIZED TRANSMISSION" && ft.report.registers[0].resultValue == "0.0000");
        CHECK(ft.report.registers[1].title == "NO NET PHASE REVERSAL" && has(ft.report.registers[1], "Dark: 2nt = (q + ½)λ"));
        CHECK(ft.report.registers[1].resultValue == "100.0000 nm" && has(ft.report.registers[0], "I_t/I_t,max = cos²(2πnt/λ)"));
        CHECK(near(std::stod(ft.report.registers[0].resultValue) + std::stod(f.report.registers[0].resultValue), 1.0, 1e-9));   // complement
        ThinFilmInput tb; tb.transmission = true; tb.thicknessNm = 200;                                     // 2nt = λ: bright
        CHECK(near(std::stod(WE::thinFilm(tb).report.registers[0].resultValue), 1.0, 1e-9));
        CHECK(near(WE::thinFilm(tb).chart.marker[0].second, 1.0, 1e-9));
        // --- second mode tabs: grating orders, single-slit geometry, Brewster reflectance
        GratingInput gl; gl.orderList = true;
        auto glr = WE::grating(gl);
        CHECK(glr.report.registers[0].title == "ORDER TABLE" && glr.report.registers[0].resultValue == "7 orders");
        CHECK(has(glr.report.registers[0], "±2 at ±36.87°") && glr.report.registers[1].resultValue == "3.3333");
        CHECK(glr.chart.marker.size() == 7 && glr.report.headline == "7 orders inside ±64.16°");
        CHECK(g.report.parameters.back().second == "transmission");
        SingleSlitInput sg; sg.geometry = true;
        auto sgr = WE::singleSlit(sg);
        CHECK(sgr.report.registers[0].title == "MINIMA ON THE SCREEN" && sgr.report.registers[0].resultValue == "5.0001 mm");
        CHECK(has(sgr.report.registers[0], "y₂ = ±10.0005 mm") && sgr.report.registers[1].resultValue == "7.1501 mm");
        CHECK(near(sgr.chart.marker[0].second, 5.0001, 1e-4) && sgr.chart.curve.size() == 6);
        CHECK(s.report.parameters.back().second == "intensity");
        PolarizationInput pb; pb.brewster = true;
        auto pbr = WE::polarization(pb);
        CHECK(pbr.report.registers[0].resultValue == "56.3099°" && pbr.report.registers[1].resultValue == "0.1479");
        CHECK(has(pbr.report.registers[1], "R_p = 0.0000   (p-polarized, no reflection)"));
        CHECK(near(pbr.chart.marker[0].second, 0.0, 1e-12) && near(pbr.chart.marker[0].first, 56.3099, 1e-3));
        CHECK(near(pbr.chart.curve.front().second, 0.04, 1e-9) && near(pbr.chart.curve.back().second, 1.0, 1e-6));   // normal incidence → grazing
        CHECK(p.report.parameters.back().second == "malus");
        // every screen: the paused report keeps the last valid result
        ReportContext ctx{&nr, "RUN 007"};
        NewtonInput zero; zero.radiusM = 0;
        auto kept = WE::newtonRings(zero, ctx);
        CHECK(kept.report.registers.size() == 2 && kept.report.registers[1].title == "LAST VALID / RUN 007" && kept.report.registers[1].resultValue == "1.5492 mm");
        // number helpers used above
        CHECK(NumberFormat::scientificCompact(5e-7) == "5×10⁻⁷" && NumberFormat::scientificCompact(1e-4) == "10⁻⁴" && NumberFormat::scientificCompact(600e-9) == "6×10⁻⁷");
    }

    // ====================================================================== v2.8 experiment extras
    {
        namespace WE = WaveExperiments;
        namespace TL = ThickLens;
        auto has = [](const LcdRegister& r, const std::string& text) { for (const auto& l : r.lines) if (l == text) return true; return false; };

        // ---- thick lens: power, principal planes and the thin-lens limit
        const TL::Result tl = TL::analyse({});
        CHECK(tl.valid && tl.converging);
        CHECK(near(tl.power, 0.096667, 1e-5) && near(tl.focalLength, 10.3448, 1e-3));
        CHECK(near(tl.h1 + tl.h2, 0.0, 1e-9));                                     // a symmetric biconvex element
        CHECK(near(tl.backFocal, tl.focalLength + tl.h2, 1e-9));                   // BFL is measured from V2
        CHECK(tl.element == "Biconvex (converging)" && !tl.atInfinity && tl.imageFromH2 > 0 && tl.magnification < 0);
        ThickLensInput thin = {}; thin.thickness = 0.0;                            // t = 0 must reduce to the thin lens
        const TL::Result lim = TL::analyse(thin);
        CHECK(near(lim.focalLength, 1.0 / (0.5 * (1.0 / 10.0 - 1.0 / -10.0)), 1e-9));
        CHECK(near(lim.h1, 0.0, 1e-12) && near(lim.h2, 0.0, 1e-12));
        CHECK(near(lim.imageFromH2, TL::thinLensImage(30.0, lim.focalLength), 1e-9));
        // spherical aberration does not vanish when t -> 0: it is the APERTURE that causes it, so a small
        // enough aperture is the only "negligible" case
        CHECK(lim.saClass == "UNDER-CORRECTED");
        ThickLensInput narrow = {}; narrow.diameter = 0.5;
        CHECK(TL::analyse(narrow).saClass == "NEGLIGIBLE");

        // ---- spherical aberration: grows with the aperture squared and points ahead of the paraxial focus
        ThickLensInput small = {}, big = {};
        small.diameter = 2.5; big.diameter = 10.0;
        const double saSmall = TL::analyse(small).longitudinalSa, saBig = TL::analyse(big).longitudinalSa;
        CHECK(near(saBig / saSmall, 16.0, 1e-6));                                  // ΔSA ∝ h²
        CHECK(saSmall < 0 && TL::analyse(big).saClass == "UNDER-CORRECTED");       // marginal rays cross first
        CHECK(near(TL::analyse(big).marginalFocus, TL::analyse(big).backFocal + saBig, 1e-12));

        // ---- invalid elements are refused, not silently calculated
        ThickLensInput window = {}; window.r1 = 1e9; window.r2 = 1e9;
        CHECK(!TL::analyse(window).valid && !TL::analyse(window).error.empty());
        ThickLensInput glass = {}; glass.n = 0.0;
        CHECK(!TL::analyse(glass).valid);

        // ---- the report of the thick-lens screen
        const Report tlr = ER::thickLens({});
        CHECK(tlr.valid && !tlr.paused && tlr.headline == "dᵢ = +15.6049 cm");
        CHECK(tlr.registers[0].title == "THICK-LENS POWER / SI" && has(tlr.registers[0], "f = 1/P = +10.3448 cm"));
        CHECK(tlr.registers[1].title == "PRINCIPAL PLANES / cm" && has(tlr.registers[1], "IMAGE: REAL · INVERTED"));
        CHECK(tlr.notices[0].body.find("UNDER-CORRECTED") != std::string::npos);
        CHECK(tlr.retained.resultValue == "+10.3448 cm" && ER::thickLensSweep({}).rows.size() == 12);
        ThickLensInput bad = {}; bad.thickness = -1.0;
        CHECK(ER::thickLens(bad).paused && ER::thickLens(bad).issues[0].field == "t");
        // the second tab: one element, three colours, red focuses further out than blue
        ThickLensInput spec = {}; spec.spectrum = true;
        const Report sr = ER::thickLens(spec);
        CHECK(sr.headline == "f(656.3 nm) = +10.0628 cm");
        CHECK(has(sr.registers[0], "Blue C 486.1 nm: n = 1.52237  f = +9.9118 cm"));
        CHECK(sr.registers[1].title == "ABBE NUMBER / DISPERSION" && near(std::stod(sr.registers[1].resultValue), 64.4, 0.1));

        // ---- thin film: the colour of the film over the whole visible band
        const FilmColour gold = thinFilmColour(100.0, 1.5);
        CHECK(gold.hueNm > 560 && gold.hueNm < 640 && gold.meanReflectance > 0.7);
        CHECK(gold.hex.size() == 7 && gold.hex[0] == '#');
        CHECK(gold.lines.size() == 7 && near(gold.lines[4].reflectance, 1.0, 1e-9));         // 600 nm is a maximum
        const FilmColour pale = thinFilmColour(2000.0, 1.5);
        CHECK(pale.hueNm == 0.0 && thinFilmColourVisibility(2000.0, 1.5) < 0.01);            // thick film: grey
        CHECK(thinFilmColourVisibility(100.0, 1.5) > 0.9);                                   // thin film: saturated
        CHECK(near(thinFilmReflectance(200.0, 1.0, 400.0), 0.0, 1e-12));                     // 2nt = λ: dark
        ThinFilmInput colours; colours.colours = true;
        const ModelReport cr = WE::thinFilm(colours);
        CHECK(cr.report.headline == "film 100 nm · 600 nm" && cr.report.registers[0].resultValue == "600 nm dominant");
        CHECK(cr.report.registers[1].resultValue == cr.report.parameters[2].second);
        CHECK(cr.chart.curve.size() == 401 && near(cr.chart.curve[0].first, 400.0) && near(cr.chart.curve.back().first, 700.0));

        // ---- single slit + a second slit: envelope, fringes and the missing order
        CHECK(near(WE::doubleSlitEnvelopeIntensity(0.0, 0.1, 0.4, 500.0), 1.0, 1e-9));
        SingleSlitInput env; env.envelope = true;
        const ModelReport er = WE::doubleSlitEnvelope(env);
        CHECK(er.report.headline == "θ_env = 0.2865°" && er.report.registers[1].resultLabel == "FRINGES INSIDE THE ENVELOPE");
        CHECK(er.report.registers[1].resultValue == "7 bright" && er.report.parameters.back().second == "envelope");
        CHECK(er.report.notices[0].body.find("m = 4 is missing") != std::string::npos);      // d/a = 4: the 4th fringe vanishes
        SingleSlitInput enBad; enBad.envelope = true; enBad.separationMm = 0.05;
        CHECK(WE::doubleSlitEnvelope(enBad).report.paused && WE::doubleSlitEnvelope(enBad).report.issues[0].field == "d");
        CHECK(near(WE::multiSlitFactor(0.0, 20), 1.0) && near(WE::multiSlitFactor(2 * 3.14159265358979, 20), 1.0, 1e-9));

        // ---- grating resolving power and the free spectral range
        const ModelReport gr = WE::gratingResolution({});
        CHECK(gr.report.headline == "δλ = 5 nm" && gr.report.registers[0].resultValue == "5 nm");
        CHECK(gr.report.registers[1].resultValue == "600 nm" && gr.report.parameters.back().second == "resolution");
        CHECK(near(WE::gratingResolution({}).chart.curve[300].second, 1.0, 1e-3));           // the line peaks at λ
        GratingResolutionInput wide = {}; wide.slits = 240;                                  // twice the lines, half the gap
        CHECK(near(std::stod(std::string(WE::gratingResolution(wide).report.registers[0].resultValue).substr(0, 3)), 2.5, 1e-9));
        GratingResolutionInput highOrder = {}; highOrder.order = 2;
        CHECK(near(std::stod(WE::gratingResolution(highOrder).report.registers[1].resultValue), 300.0, 1e-9));
        GratingResolutionInput impossible = {}; impossible.order = 4;                        // d/λ = 3.33: no 4th order
        CHECK(WE::gratingResolution(impossible).report.paused);
    }

    // ====================================================================== printable report (HTML)
    {
        ReportContent content;
        content.notes = "# Notes\n- n₁ sin θ₁ = **0.5**\n- transmitted";
        ReportSection sec;
        sec.title = "Thin-Film Interference";
        sec.headline = "I_r/I_r,max = 1.0000";
        sec.parameters = {{"λ", "600 nm"}, {"n", "1.5"}};
        LcdRegister reg;
        reg.title = "OPTICAL PATH / PHASE";
        reg.lines = {"OPD = 2nt", " = 300 nm"};
        reg.resultLabel = "NORMALIZED REFLECTION";
        reg.resultValue = "1.0000";
        sec.registers = {reg};
        sec.diagramKey = reportImageKey("diagram", sec.title);
        sec.chartKey = reportImageKey("chart", sec.title);
        content.sections = {sec};
        RunRecord sample;
        sample.id = 1;
        sample.experiment = "Prisms";
        sample.description = "A = 60°";
        sample.result = "δ = 38.5°";
        sample.valid = true;
        content.runs = {sample};

        ReportOptions o;
        o.generated = "13 November 2026 · 02:15";
        const std::string html = reportHtml(content, o);
        CHECK(html.find("<h1>PHY104 Laboratory Report</h1>") != std::string::npos);
        CHECK(html.find("1. Thin-Film Interference") != std::string::npos && html.find("I_r/I_r,max = 1.0000") != std::string::npos);
        CHECK(html.find("img://diagram-thin-film-interference") != std::string::npos);
        CHECK(html.find("img://chart-thin-film-interference") != std::string::npos);
        CHECK(html.find("width=\"520\"") != std::string::npos);
        CHECK(html.find("NORMALIZED REFLECTION") != std::string::npos && html.find("OPD = 2nt") != std::string::npos);
        CHECK(html.find("<h3>Notes</h3>") != std::string::npos && html.find("<li>n₁ sin θ₁ = <b>0.5</b></li>") != std::string::npos);
        CHECK(html.find("Saved calculations") != std::string::npos && html.find("<td>Prisms</td>") != std::string::npos);
        CHECK(html.find("13 November 2026 · 02:15") != std::string::npos);

        ReportOptions none = o;
        none.notes = none.registers = none.diagrams = none.charts = none.runs = false;
        const std::string bare = reportHtml(content, none);
        CHECK(bare.find("img://") == std::string::npos && bare.find("NORMALIZED REFLECTION") == std::string::npos);
        CHECK(bare.find("Session notes") == std::string::npos && bare.find("Saved calculations") == std::string::npos);
        CHECK(bare.find("Thin-Film Interference") != std::string::npos && bare.find("Nothing selected yet") == std::string::npos);

        ReportOptions narrow = o;
        narrow.imageWidth = 320;
        CHECK(reportHtml(content, narrow).find("width=\"320\"") != std::string::npos);

        ReportContent risky;                                                    // user text must not break the document
        ReportSection bad;
        bad.title = "A < B & C";
        bad.headline = "\"quoted\"";
        risky.sections = {bad};
        const std::string safe = reportHtml(risky, o);
        CHECK(safe.find("A &lt; B &amp; C") != std::string::npos && safe.find("&quot;quoted&quot;") != std::string::npos);

        CHECK(markdownToHtml("### RUN 007") == "<h5>RUN 007</h5>" && markdownToHtml("# Notes") == "<h3>Notes</h3>");
        CHECK(markdownToHtml("- a\n- b") == "<ul><li>a</li><li>b</li></ul>");
        CHECK(markdownToHtml("use `Ans` here") == "<p>use <span style=\"font-family:monospace\">Ans</span> here</p>");
        CHECK(markdownToHtml("a < b") == "<p>a &lt; b</p>");
        CHECK(markdownToHtml("") .empty());
        CHECK(reportFileStem("2026-11-13") == "PHY104_report_2026-11-13" && reportFileStem("") == "PHY104_report");
        CHECK(reportImageKey("chart", "Newton's Rings") == "img://chart-newton-s-rings");
    }

    if (g_failures == 0) std::cout << "All core tests passed\n";
    return g_failures == 0 ? 0 : 1;
}
