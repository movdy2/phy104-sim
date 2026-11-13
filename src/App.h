#pragma once
// Builds the whole application: data layer -> business layer -> presentation layer.
// main() and the tests use the same class, so tests exercise the real thing.
#include <QString>
#include <QStringList>
#include <memory>

namespace phy104 {

class AppContext;
class ConstantsPage;
class DashboardPage;
class DiagramsPage;
class EquationLabPage;
class ExportPage;
class GratingPage;
class MichelsonPage;
class NewtonRingsPage;
class PolarizationPage;
class SingleSlitPage;
class ThinFilmPage;
class FormulaEngine;
class FormulaManager;
class LensesPage;
class MainWindow;
class MathSolverPage;
class NotesPage;
class PrismsPage;
class ReportPage;
class RefractionPage;
class SolverEngine;
class SolverPage;
class WavePage;

class App {
public:
    struct Paths {
        QString dataDir;          // folder with the JSON files
        QString sessionFile;      // the autosaved .phy file
    };

    /// Throws std::runtime_error when a data file is missing or invalid.
    explicit App(const Paths& paths);
    ~App();

    MainWindow& window() { return *m_window; }
    AppContext& context() { return *m_context; }

    // pages (owned by the window)
    DashboardPage* dashboard = nullptr;
    RefractionPage* refraction = nullptr;
    LensesPage* lenses = nullptr;
    PrismsPage* prisms = nullptr;
    WavePage* wave = nullptr;
    PolarizationPage* polarization = nullptr;
    SingleSlitPage* singleSlit = nullptr;
    GratingPage* grating = nullptr;
    ThinFilmPage* thinFilm = nullptr;
    MichelsonPage* michelson = nullptr;
    NewtonRingsPage* newton = nullptr;
    EquationLabPage* equationLab = nullptr;
    NotesPage* notes = nullptr;
    ExportPage* exportPage = nullptr;
    ReportPage* reportPage = nullptr;
    SolverPage* solver = nullptr;
    MathSolverPage* calculator = nullptr;
    ConstantsPage* constants = nullptr;
    DiagramsPage* diagrams = nullptr;

    /// Warnings found while loading the data files (shown once at start-up).
    QStringList dataWarnings;

private:
    std::unique_ptr<AppContext> m_context;
    std::shared_ptr<FormulaEngine> m_engine;
    std::shared_ptr<FormulaManager> m_manager;
    std::shared_ptr<SolverEngine> m_solver;
    std::unique_ptr<MainWindow> m_window;
};

} // namespace phy104
