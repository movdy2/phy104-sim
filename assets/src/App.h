#pragma once
// يبني التطبيق بالكامل: طبقة البيانات -> طبقة الأعمال -> طبقة العرض.
// main() والاختبارات يستخدمان نفس الكلاس، لذا الاختبارات تختبر التطبيق الحقيقي.
#include <QString>
#include <QStringList>
#include <memory>

namespace phy104 {

// إعلانات مسبقة (Forward Declarations) لتقليل وقت الترجمة (Compile Time)
class AppContext;
class ConstantsPage;
class DashboardPage;
class DiagramsPage;
class FormulaEngine;
class FormulaManager;
class LensesPage;
class MainWindow;
class MathSolverPage;
class NotesExportPage;
class PrismsPage;
class RefractionPage;
class SolverEngine;
class SolverPage;
class WavePage;

class App {
public:
    /// مسارات التطبيق: مجلد البيانات وملف الجلسة
    struct Paths {
        QString dataDir;          // مجلد يحتوي على ملفات JSON
        QString sessionFile;      // ملف الجلسة (.phy) المحفوظ تلقائياً
    };

    /// الباني: يرمي std::runtime_error إذا كان هناك ملف بيانات مفقود أو غير صالح.
    explicit App(const Paths& paths);
    ~App();

    // دوال الوصول (Accessors)
    MainWindow& window() { return *m_window; }
    AppContext& context() { return *m_context; }

    // الصفحات (تملكها النافذة الرئيسية)
    DashboardPage* dashboard = nullptr;
    RefractionPage* refraction = nullptr;
    LensesPage* lenses = nullptr;
    PrismsPage* prisms = nullptr;
    WavePage* wave = nullptr;
    NotesExportPage* notes = nullptr;
    SolverPage* solver = nullptr;
    MathSolverPage* calculator = nullptr;
    ConstantsPage* constants = nullptr;
    DiagramsPage* diagrams = nullptr;

    /// تحذيرات وُجدت أثناء تحميل ملفات البيانات (تُعرض مرة واحدة عند بدء التشغيل).
    QStringList dataWarnings;

private:
    // الحالة العامة (الجلسة، الإعدادات)
    std::unique_ptr<AppContext> m_context;

    // محركات الحساب (مشتركة بين عدة صفحات)
    std::shared_ptr<FormulaEngine> m_engine;
    std::shared_ptr<FormulaManager> m_manager;
    std::shared_ptr<SolverEngine> m_solver;

    // النافذة الرئيسية
    std::unique_ptr<MainWindow> m_window;
};

} // namespace phy104