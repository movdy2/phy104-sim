#include "App.h"

#include "FormulaEngine.h"
#include "FormulaManager.h"
#include "JsonRepositories.h"
#include "SolverEngine.h"
#include "ui/AppContext.h"
#include "ui/BasePage.h"
#include "ui/ConstantsPage.h"
#include "ui/DashboardPage.h"
#include "ui/DiagramsPage.h"
#include "ui/EquationLabPage.h"
#include "ui/ExportPage.h"
#include "ui/ExperimentPages.h"
#include "ui/ModelExperimentPages.h"
#include "ui/MainWindow.h"
#include "ui/MathSolverPage.h"
#include "ui/NotesPage.h"
#include "ui/ReportPage.h"
#include "ui/SolverPage.h"
#include "ui/StudyHubPage.h"
#include "ui/ThickLensPage.h"

namespace phy104 {

App::App(const Paths& paths) {
    const QString d = paths.dataDir;

    // ---- data layer
    JsonVariableRepository variablesRepo(d + "/variables.json");
    JsonConstantsRepository constantsRepo(d + "/constant.json");
    JsonChapterRepository chapterRepo(d + "/chapters.json");
    JsonKeyboardRepository keyboardRepo(d + "/keyboard.json");
    JsonPresetsRepository presetsRepo(d + "/optics_presets.json");
    JsonReferenceRepository referenceRepo(d + "/reference.json");
    JsonStudyHubRepository studyHubRepo(d + "/study_hub.json");

    // ---- business layer
    m_engine = std::make_shared<FormulaEngine>(std::make_unique<JsonFormulaRepository>(d + "/formulas.json"),
                                               variablesRepo, constantsRepo);
    m_manager = std::make_shared<FormulaManager>(*m_engine, chapterRepo.loadAll());
    m_solver = std::make_shared<SolverEngine>(constantsRepo.loadAll());
    for (const auto& w : m_engine->warnings()) dataWarnings << QString::fromStdString(w);

    m_context = std::make_unique<AppContext>(presetsRepo.load(), referenceRepo.loadAll(), referenceRepo.unitDiscipline(),
                                             std::make_unique<JsonSessionStore>(paths.sessionFile), "optics_lab.phy");

    // ---- presentation layer: the order is the sidebar order of the Figma design
    m_window = std::make_unique<MainWindow>(m_context.get(), keyboardRepo.loadAll());
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
    polarization = new PolarizationPage(m_context.get());
    m_window->addPage("Polarization (Malus & Brewster)", "contrast", polarization);         // 5
    singleSlit = new SingleSlitPage(m_context.get());
    m_window->addPage("Single-Slit Diffraction", "slit", singleSlit);                      // 6
    grating = new GratingPage(m_context.get());
    m_window->addPage("Diffraction Grating", "align-justify", grating);                     // 7
    thinFilm = new ThinFilmPage(m_context.get());
    m_window->addPage("Thin-Film Interference", "layers", thinFilm);                        // 8
    michelson = new MichelsonPage(m_context.get());
    m_window->addPage("Michelson Interferometry", "git-merge", michelson);                  // 9
    newton = new NewtonRingsPage(m_context.get());
    m_window->addPage("Newton's Rings", "circle-dot", newton);                              // 10
    thickLens = new ThickLensPage(m_context.get());
    m_window->addPage("Thick Lens & Aberration", "aperture", thickLens);                    // 11
    notes = new NotesPage(m_context.get(), keyboardRepo.scientific());
    const int notesIndex = m_window->addPage("Notes", "notebook-pen", notes, 1);             // standalone Notes shortcut
    equationLab = new EquationLabPage(m_context.get(), keyboardRepo.scientific());
    m_window->addPage("Equation Lab", "square-function", equationLab, 1);
    constants = new ConstantsPage(constantsRepo);
    diagrams = new DiagramsPage;
    studyHub = new StudyHubPage(studyHubRepo.loadAll(), m_context.get(), constants, diagrams);
    m_window->addPage("Study Hub", "book-open", studyHub, 1);                                // chapters, notes, setups, constants
    solver = new SolverPage(m_engine, m_manager, m_solver, m_context.get());
    m_window->addPage("Formula Solver", "sigma", solver, 1);
    calculator = new MathSolverPage(m_context.get());
    m_window->addPage("Calculator", "calculator", calculator, 1);
    exportPage = new ExportPage(m_context.get());
    const int exportIndex = m_window->addPage("Export (Excel/CSV)", "file-spreadsheet", exportPage, 1);   // tools entry, as in Figma
    reportPage = new ReportPage(m_context.get());
    const int reportIndex = m_window->addPage("PDF Report", "printer", reportPage, 1);
    reportPage->setSources({refraction, lenses, prisms, wave, polarization, singleSlit, grating, thinFilm, michelson, newton, thickLens});
    dashboard->setNotesPage(notesIndex);
    dashboard->setExportPage(exportIndex);

    // Route each shared page-header Export action; the destination pages hide their own duplicate menu.
    for (int i = 0; i < m_window->pageCount(); ++i) {
        auto* page = qobject_cast<BasePage*>(m_window->page(i));
        if (!page) continue;
        QObject::connect(page, &BasePage::exportFormatRequested, m_window.get(), [this, exportIndex, reportIndex](int format) {
            if (format == 0) { m_window->setCurrentPage(reportIndex); return; }
            exportPage->setFormat(format == 1);            // 1 = Excel (.xlsx), 2 = CSV (.csv)
            m_window->setCurrentPage(exportIndex);
        });
    }

    QObject::connect(dashboard, &BasePage::openPageRequested, m_window.get(), &MainWindow::setCurrentPage);
}

App::~App() = default;

} // namespace phy104
