#include "App.h"

#include "FormulaEngine.h"
#include "FormulaManager.h"
#include "JsonRepositories.h"
#include "SolverEngine.h"
#include "ui/AppContext.h"
#include "ui/ConstantsPage.h"
#include "ui/DashboardPage.h"
#include "ui/DiagramsPage.h"
#include "ui/ExperimentPages.h"
#include "ui/MainWindow.h"
#include "ui/MathSolverPage.h"
#include "ui/NotesExportPage.h"
#include "ui/SolverPage.h"

// استخدام using في ملف .cpp للراحة (مسموح)
using std::make_shared;
using std::make_unique;
using std::string;

namespace phy104 {

/// الباني (Composition Root): يبني كل شيء بالترتيب:
/// 1. طبقة البيانات (JSON Repositories)
/// 2. طبقة الأعمال (Formula Engine, Manager, Solver)
/// 3. طبقة العرض (MainWindow + الصفحات)
App::App(const Paths& paths) {
    const QString d = paths.dataDir;

    // ============================================================
    // 1. طبقة البيانات: مستودعات JSON
    // ============================================================
    JsonVariableRepository variablesRepo(d + "/variables.json");
    JsonConstantsRepository constantsRepo(d + "/constant.json");
    JsonChapterRepository chapterRepo(d + "/chapters.json");
    JsonKeyboardRepository keyboardRepo(d + "/keyboard.json");
    JsonPresetsRepository presetsRepo(d + "/optics_presets.json");
    JsonReferenceRepository referenceRepo(d + "/reference.json");
    JsonSummaryRepository summaryRepo(d + "/notes.json");
    // ============================================================
    // 2. طبقة الأعمال: محركات الحساب
    // ============================================================
    m_engine = make_shared<FormulaEngine>(
        make_unique<JsonFormulaRepository>(d + "/formulas.json"),
        variablesRepo, constantsRepo);

    m_manager = make_shared<FormulaManager>(*m_engine, chapterRepo.loadAll());

    m_solver = make_shared<SolverEngine>(constantsRepo.loadAll());

    // جمع تحذيرات بناء المعادلات (مثلاً متغير مفقود في variables.json)
    for (const auto& w : m_engine->warnings()) dataWarnings << QString::fromStdString(w);

    // ============================================================
    // 3. طبقة العرض: AppContext (الحالة العامة) + MainWindow
    // ============================================================
    m_context = make_unique<AppContext>(
        presetsRepo.load(),
        referenceRepo.loadAll(),
        referenceRepo.unitDiscipline(),
        make_unique<JsonSessionStore>(paths.sessionFile),
        "optics_lab.phy");

    m_window = make_unique<MainWindow>(m_context.get(), keyboardRepo.loadAll());

    // ============================================================
    // 4. بناء الصفحات بالترتيب الذي تريده في الشريط الجانبي
    // ============================================================
    dashboard = new DashboardPage(m_context.get());
    m_window->addPage("Dashboard", "layout-dashboard", dashboard);                          // 0

    refraction = new RefractionPage(m_context.get());
    m_window->addPage("Refraction & TIR", "split", refraction);                             // 1

    lenses = new LensesPage(m_context.get());
    m_window->addPage("Lenses & Mirrors", "focus", lenses);                                 // 2

    prisms = new PrismsPage(m_context.get());
    m_window->addPage("Prisms", "triangle", prisms);                                        // 3

    wave = new WavePage(m_context.get());
    m_window->addPage("Wave Interference (Double-Slit)", "waves", wave);                    // 4

    notes = new NotesExportPage(m_context.get());
    const int notesIndex = m_window->addPage("Formulas & Notes", "notebook-pen", notes);    // 5
    m_window->addNavAlias("Export (Excel/CSV)", "file-spreadsheet", notesIndex);

    solver = new SolverPage(m_engine, m_manager, m_solver);
    m_window->addPage("Formula Solver", "sigma", solver, 1);                                // 6

    calculator = new MathSolverPage;
    m_window->addPage("Calculator", "calculator", calculator, 1);                           // 7

    constants = new ConstantsPage(constantsRepo);
    m_window->addPage("Constants", "ruler", constants, 1);                                  // 8

    diagrams = new DiagramsPage;
    m_window->addPage("Optical Setups", "image", diagrams, 1);                              // 9

    // ============================================================
    // 5. ربط الإشارات (Signals): مثلاً صفحة Dashboard تطلب الانتقال لصفحة
    // ============================================================
    QObject::connect(dashboard, &BasePage::openPageRequested, m_window.get(), &MainWindow::setCurrentPage);
}

App::~App() = default;

} // namespace phy104
