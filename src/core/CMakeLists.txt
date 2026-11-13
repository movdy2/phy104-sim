add_library(phy104_core STATIC
    ExpressionEvaluator.cpp
    EquationSolver.cpp
    SolverEngine.cpp
    FormulaEngine.cpp
    PhysicsCalculator.cpp
    WaveExperiments.cpp
    ThickLens.cpp
    Spectrum.cpp
    FormulaManager.cpp
    MathCalculator.cpp
    LatexFormatter.cpp
    NumberFormat.cpp
    ExperimentReports.cpp
    ReportSupport.cpp
    ReportDocument.cpp
    SessionLog.cpp
    XlsxWriter.cpp
    DataExport.cpp
    EquationText.cpp
    Units.cpp
)
target_include_directories(phy104_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
phy104_enable_warnings(phy104_core STRICT)
