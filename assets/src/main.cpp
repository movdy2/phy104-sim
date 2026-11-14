// نقطة التجميع (Composition Root): المكان الوحيد الذي يعرف الكلاسات الفعلية لكل الطبقات.
#include "App.h"
#include "ui/AppContext.h"
#include "ui/Components.h"
#include "ui/ExperimentPages.h"
#include "ui/MainWindow.h"
#include "ui/MarkdownWorkspace.h"
#include "ui/MathSolverPage.h"
#include "ui/NotesExportPage.h"
#include "ui/SolverPage.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>

// ============================================================
// دالة مساعدة للمطور: PHY104OpticalAssistant --screenshots <folder>
// تتحكم في الواجهة (كتابة، نقرات، تبديل أوضاع) وتحفظ صورة PNG لكل صفحة وحالة، ثم تخرج.
// ============================================================
static void takeScreenshots(QApplication& app, phy104::App& application, const QString& folder) {
    using namespace phy104;
    MainWindow& window = application.window();
    QDir().mkpath(folder);
    window.resize(1280, 800);
    window.show();

    // التقاط صورة للشاشة الحالية
    auto grab = [&](const QString& name) {
        app.processEvents(); app.processEvents();
        window.grab().save(folder + "/" + name + ".png");
    };

    // النقر على زر يحمل نصاً معيناً
    auto click = [&](QWidget* root, const QString& text) {
        for (auto* b : root->findChildren<QPushButton*>())
            if (b->text() == text && b->isVisibleTo(&window)) { b->click(); return; }
    };

    // --- محتوى تجريبي ---
    application.solver->selectFormula("snell_law");
    auto edits = application.solver->findChildren<QLineEdit*>("lcdInput");
    if (edits.size() >= 4) {
        edits[0]->setText("1"); edits[1]->setText("40°"); edits[2]->setText("1.5"); edits[3]->clear();
        click(application.solver, "Solve");
    }

    if (auto* input = application.calculator->findChild<QLineEdit*>("lcdInput")) {
        for (const char* expr : {"1+1+1+2-4/24", "sin(30)+cos(60)*sqrt(16)", "2*pi", "Ans/2"}) {
            input->setText(expr);
            QMetaObject::invokeMethod(input, "returnPressed");
        }
    }

    if (auto* notes = application.notes->findChild<MarkdownWorkspace*>()) {
        notes->setMarkdown("# PHY104 · Optics notes\n\n## Chapter 1 · Refraction\n\n### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$\n- light bends towards the normal in a denser medium\n"
                           "- critical angle: $sin θc = n₂ / n₁$ (only if n₁ > n₂)\n\n### Worked example\n**Given:**\n- n₁ = 1.5, n₂ = 1.0, θ₁ = 30°\n\n"
                           "**Substitution & Solution:**\n- sin θ₂ = (1.5 × 0.5) / 1.0 = 0.75 → θ₂ = arcsin(0.75) = 48.59°\n\n#### Exam warning\nAngles are measured from the normal, not from the surface.\n");
    }

    // التقاط كل الصفحات في الوضعين (داكن / فاتح)
    for (bool dark : {true, false}) {
        ThemeManager::instance().apply(app, dark);
        for (int i = 0; i < window.pageCount(); ++i) {
            window.setCurrentPage(i);
            grab(QString("%1_%2").arg(i, 2, 10, QChar('0')).arg(dark ? "dark" : "light"));
        }
    }
    ThemeManager::instance().apply(app, true);

    // --- الحالات الجديدة للتصميم ---
    window.setCurrentPage(1);
    click(application.refraction, "TIR");
    grab("state_tir");
    click(application.refraction, "Refraction");

    window.setCurrentPage(2);
    click(application.lenses, "Mirror");
    for (auto* f : application.lenses->findChildren<UnitField*>())
        if (f->isVisibleTo(&window)) { f->setText("-10"); emit f->edited(); break; }
    grab("state_mirror");
    click(application.lenses, "Thin lens");

    window.setCurrentPage(4);
    for (auto* f : application.wave->findChildren<UnitField*>())
        if (f->unit() == "mm") { f->setText("0"); emit f->edited(); break; }
    grab("state_paused");

    window.setCurrentPage(5);
    if (auto* notes = application.notes->findChild<MarkdownWorkspace*>()) {
        notes->setMode(MarkdownWorkspace::Edit);
        grab("state_notes_edit");
        notes->setMode(MarkdownWorkspace::Split);
    }
    window.setCurrentPage(7);
    grab("state_calculator");
}

// ============================================================
// نقطة البداية (Entry Point)
// ============================================================
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 1. تحميل الخطوط وتطبيق الثيم الداكن (Figma Dark)
    phy104::ThemeManager::loadFonts();
    phy104::ThemeManager::instance().apply(app, true);

    try {
        // 2. بناء التطبيق (Composition Root)
        phy104::App application({
            QCoreApplication::applicationDirPath() + "/data",
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/optics_lab.phy"
        });

        // 3. عرض تحذيرات البيانات (إن وجدت)
        if (!application.dataWarnings.isEmpty())
            QMessageBox::warning(nullptr, "PHY104 Optical Assistant",
                                 "Problems found in the data files:\n- " + application.dataWarnings.join("\n- "));

        phy104::MainWindow& window = application.window();

        // 4. ربط زر تبديل الثيم
        QObject::connect(&window, &phy104::MainWindow::themeToggled,
                         [&app](bool dark) { phy104::ThemeManager::instance().apply(app, dark); });

        // 5. وضع المطور: أخذ لقطات الشاشة والخروج
        const int shot = app.arguments().indexOf("--screenshots");
        if (shot >= 0 && shot + 1 < app.arguments().size()) {
            takeScreenshots(app, application, app.arguments().at(shot + 1));
            return 0;
        }

        // 6. عرض النافذة وتشغيل حلقة الأحداث (Event Loop)
        window.show();
        const int code = app.exec();

        // 7. حفظ الجلسة تلقائياً عند الخروج
        application.context().saveSession();
        return code;
    } catch (const std::exception& e) {
        // 8. معالجة أي خطأ فادح أثناء البناء (مثل ملف بيانات مفقود)
        QMessageBox::critical(nullptr, "PHY104 Optical Assistant", e.what());
        return 1;
    }
}