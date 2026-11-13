// Drives the REAL application with simulated mouse clicks and key presses (QtTest) and checks
// that every control does something. This is the regression test for "a button does not work".
#include "App.h"
#include "ui/AppContext.h"
#include "ui/Components.h"
#include "ui/ConstantsPage.h"
#include "ui/DashboardPage.h"
#include "ui/DiagramsPage.h"
#include "ui/ExperimentPages.h"
#include "ui/MainWindow.h"
#include "ui/EquationLabPage.h"
#include "ui/ExportPage.h"
#include "ui/MarkdownHighlighter.h"
#include "ui/ModelExperimentPages.h"
#include "ui/ScientificKeyboard.h"
#include "ui/MarkdownWorkspace.h"
#include "ui/MathSolverPage.h"
#include "ui/NotesExportPage.h"
#include "ui/ReportPage.h"
#include "ui/SolverPage.h"
#include "ui/ThemeManager.h"
#include "ui/VirtualKeyboard.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDate>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextFragment>
#include <QToolButton>
#include <algorithm>

using namespace phy104;

namespace {

QPushButton* findButton(QWidget* root, const QString& text) {
    for (auto* b : root->findChildren<QPushButton*>())
        if (b->text().trimmed() == text && b->isVisibleTo(root->window())) return b;
    return nullptr;
}

void click(QWidget* w) {
    QVERIFY2(w, "widget to click not found");
    QVERIFY2(w->isVisibleTo(w->window()), qPrintable(w->metaObject()->className() + QString(" is not visible")));
    QVERIFY2(w->isEnabled(), qPrintable(w->metaObject()->className() + QString(" is disabled")));
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, w->rect().center());
}

/// Types text like a keyboard does, including characters outside Latin-1 (QTest::keyClicks cannot send θ or ₁).
void typeText(QWidget* w, const QString& text) {
    for (const QChar ch : text) {
        if (ch.unicode() < 128) QTest::keyClick(w, ch.toLatin1());
        else QTest::sendKeyEvent(QTest::Click, w, Qt::Key_unknown, QString(ch), Qt::NoModifier);
    }
}

/// Click into the field, replace its text by typing (no Enter!).
void typeInto(UnitField* f, const QString& text) {
    QTest::mouseClick(f->edit(), Qt::LeftButton);
    f->edit()->selectAll();
    QTest::keyClicks(f->edit(), text);
}

QStringList allLabelTexts(QWidget* root) {
    QApplication::processEvents();                              // newly created labels are shown by the event loop
    QStringList out;
    for (auto* l : root->findChildren<QLabel*>()) if (l->isVisibleTo(root->window())) out << l->text();
    return out;
}

QString bigValue(QWidget* page, int index = 0) {
    QApplication::processEvents();
    QList<QLabel*> bigs;
    for (auto* l : page->findChildren<QLabel*>("lcdBig")) if (l->isVisibleTo(page->window())) bigs << l;
    return index < bigs.size() ? bigs[index]->text() : QString();
}

/// The real tab buttons inside a ModeTabs strip (objectName "modeTab").
QPushButton* modeTab(QWidget* page, const QString& label) {
    for (auto* b : page->findChildren<QPushButton*>(QStringLiteral("modeTab")))
        if (b->text() == label) return b;
    return nullptr;
}

QComboBox* comboWith(QWidget* page, const QString& itemText) {
    for (auto* c : page->findChildren<QComboBox*>())
        for (int i = 0; i < c->count(); ++i)
            if (c->itemText(i) == itemText) return c;
    return nullptr;
}

/// Picks an item the way the pages listen to it: `activated` is only emitted for user choices.
void chooseItem(QComboBox* c, int index) {
    QVERIFY2(c, "combo not found");
    c->setCurrentIndex(index);
    c->activated(index);
}

} // namespace

class InteractionTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_tmp;
    std::unique_ptr<App> m_app;

    MainWindow& win() { return m_app->window(); }
    void go(int page) { win().setCurrentPage(page); QApplication::processEvents(); }
    int idx(QWidget* w) { return win().indexOf(w); }
    void goTo(QWidget* w) { go(idx(w)); }

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        ThemeManager::loadFonts();
        ThemeManager::instance().apply(*qApp, true);
        m_app = std::make_unique<App>(App::Paths{DATA_DIR, m_tmp.path() + "/session.phy"});
        win().resize(1280, 800);
        win().show();
        QVERIFY(QTest::qWaitForWindowExposed(&win()));
    }
    void cleanupTestCase() { m_app.reset(); }

    // ------------------------------------------------------------------ navigation
    void sidebarButtonsOpenTheirPages() {
        const auto buttons = win().navButtons();
        QVERIFY(buttons.size() >= 10);
        for (QPushButton* b : buttons) {
            click(b);
            QVERIFY2(b->isChecked(), qPrintable("nav button not checked: " + b->text()));
        }
        click(buttons[1]);
        QCOMPARE(win().currentPage(), 1);
        click(buttons[4]);
        QCOMPARE(win().currentPage(), 4);
        click(buttons[0]);
        QCOMPARE(win().currentPage(), 0);
    }
    void dashboardCardsAndButtonsNavigate() {
        go(0);
        const auto cards = m_app->dashboard->findChildren<QFrame*>("cardHot");
        QCOMPARE(cards.size(), 4);
        for (int i = 0; i < 4; ++i) {
            go(0);
            click(cards[i]);
            QCOMPARE(win().currentPage(), i + 1);
        }
        go(0);
        click(findButton(m_app->dashboard, "Open notes"));
        QCOMPARE(win().currentPage(), idx(m_app->notes));
        go(0);
        click(findButton(m_app->dashboard, "Export data"));
        QCOMPARE(win().currentPage(), idx(m_app->exportPage));
    }
    void dashboardComparesAndDeletesSavedRuns() {
        AppContext& ctx = m_app->context();
        const int before = ctx.log.savedCalculations();
        for (int i = 0; i < 2; ++i) {
            Snapshot snap;
            snap.experiment = "Prism sweep";
            snap.series.name = i == 0 ? "Sweep A" : "Sweep B";
            snap.series.headers = {"d_o (mm)", "d_i (mm)"};
            snap.series.rows = {{10.0, 15.0 + i}, {20.0, 12.0 + i}, {30.0, 10.0 + i}};
            ctx.publish(snap);
            ctx.recordRun(QStringLiteral("Prism sweep"), i == 0 ? "first setting" : "second setting", "n = 1.5", true);
        }
        QCOMPARE(ctx.log.savedCalculations(), before + 2);
        const int newest = ctx.log.runs().back().id;
        QVERIFY(ctx.log.runs().back().id > 0 && ctx.log.runs().back().series.rows.size() == 3);   // the sweep travelled with the run
        go(0);

        // the "Compare" tab overlays two saved sweeps on one chart
        click(modeTab(m_app->dashboard, "Compare"));
        QComboBox* pickA = nullptr;
        QComboBox* pickB = nullptr;
        for (auto* c : m_app->dashboard->findChildren<QComboBox*>()) {
            if (c->count() >= 2 && c->itemData(0).isValid()) (pickA ? pickB : pickA) = c;
        }
        QVERIFY2(pickA && pickB, "the compare tab has two run pickers");
        chooseItem(pickA, pickA->count() - 2);
        chooseItem(pickB, pickB->count() - 1);
        QApplication::processEvents();
        bool summary = false;
        for (auto* l : m_app->dashboard->findChildren<QLabel*>())
            if (l->text().contains(" vs ") && l->text().contains("points")) summary = true;
        QVERIFY2(summary, "the compare tab reports both runs");
        click(modeTab(m_app->dashboard, "Recent"));

        // "Delete selected run" drops the newest run and autosaves the session
        QTableWidget* table = nullptr;
        for (auto* t : m_app->dashboard->findChildren<QTableWidget*>())
            if (t->columnCount() == 4) table = t;
        QVERIFY(table);
        int row = -1;
        for (int i = 0; i < table->rowCount(); ++i)
            if (table->item(i, 0) && table->item(i, 0)->data(Qt::UserRole).toInt() == newest) row = i;
        QVERIFY2(row >= 0, "the newest run is listed");
        table->setCurrentCell(row, 0);
        QApplication::processEvents();
        QPushButton* del = findButton(m_app->dashboard, "Delete selected run");
        QVERIFY(del && del->isEnabled());
        click(del);
        QApplication::processEvents();
        QCOMPARE(ctx.log.savedCalculations(), before + 1);
        QVERIFY(ctx.log.runs().back().id < newest);
        QVERIFY(QFile::exists(m_tmp.path() + "/session.phy"));       // the deletion was written to the .phy file
    }
    void themeButtonTogglesRepeatedly() {
        const bool start = ThemeManager::instance().isDark();
        for (int i = 0; i < 4; ++i) click(win().themeButton());
        QCOMPARE(ThemeManager::instance().isDark(), start);          // four toggles: back where we began
        QApplication::processEvents();
    }
    void saveSessionButtonWritesTheFile() {
        go(1);
        QFile::remove(m_tmp.path() + "/session.phy");
        click(findButton(m_app->refraction, "Save session"));
        QVERIFY(QFile::exists(m_tmp.path() + "/session.phy"));
    }

    // ------------------------------------------------------------------ experiments
    void refractionUpdatesWhileTyping() {
        go(1);
        auto fields = m_app->refraction->findChildren<UnitField*>();
        QVERIFY(fields.size() >= 3);
        const QString before = bigValue(m_app->refraction);
        typeInto(fields[2], "40");                                       // θ₁ = 40°, NO Enter / focus change
        QTest::qWait(700);
        const QString after = bigValue(m_app->refraction);
        QVERIFY2(after != before, qPrintable("register did not update while typing: " + before + " -> " + after));
        QVERIFY2(after.startsWith("74."), qPrintable(after));
    }
    void calculateButtonSavesARun() {
        go(1);
        const int runs = m_app->context().log.savedCalculations();
        click(findButton(m_app->refraction, "Calculate && Simulate"));
        QCOMPARE(m_app->context().log.savedCalculations(), runs + 1);
    }
    void resetButtonRestoresDefaults() {
        go(1);
        click(findButton(m_app->refraction, "Reset parameters"));
        auto fields = m_app->refraction->findChildren<UnitField*>();
        QCOMPARE(fields[2]->text(), QString("30.00"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->refraction), QString("48.5904°"), 1500);
    }
    void spinnerArrowsStepTheValue() {
        go(1);
        auto fields = m_app->refraction->findChildren<UnitField*>();
        const double before = fields[0]->value();
        QTest::mouseClick(fields[0]->edit(), Qt::LeftButton);
        QTest::keyClick(fields[0]->edit(), Qt::Key_Up);
        QVERIFY(fields[0]->value() > before);
        auto* up = fields[0]->findChild<QWidget*>("spinUp");             // the clickable ▲ of the ↕ control
        QVERIFY2(up, "spinner has no clickable arrows");
        const double mid = fields[0]->value();
        click(up);
        QVERIFY(fields[0]->value() > mid);
    }
    void modeTabsSwitchTheExperimentView() {
        go(1);
        click(findButton(m_app->refraction, "TIR"));
        QTest::qWait(50);
        QVERIFY2(allLabelTexts(m_app->refraction).join("|").contains("TIR", Qt::CaseSensitive), "TIR tab shows nothing");
        click(findButton(m_app->refraction, "Refraction"));
        go(2);
        click(findButton(m_app->lenses, "Mirror"));
        QTest::qWait(50);
        click(findButton(m_app->lenses, "Thin lens"));
    }
    void invalidInputPausesAndRestoreButtonWorks() {
        go(4);
        auto fields = m_app->wave->findChildren<UnitField*>();             // λ, d, L, m
        typeInto(fields[1], "0");
        QTest::qWait(700);
        auto* calc = findButton(m_app->wave, "Calculate && Simulate");
        QVERIFY(calc);
        QVERIFY2(!calc->isEnabled(), "Calculate must be disabled while the input is invalid");
        auto* restore = findButton(m_app->wave, "Restore d = 0.250 mm");
        QVERIFY2(restore, "no Restore button in the invalid state");
        click(restore);
        QTest::qWait(100);
        QCOMPARE(fields[1]->text(), QString("0.250"));
        QVERIFY(findButton(m_app->wave, "Calculate && Simulate")->isEnabled());
    }

    // ------------------------------------------------------------------ calculator
    void calculatorEnterEqualsAndKeypad() {
        goTo(m_app->calculator);
        auto* input = m_app->calculator->findChild<QLineEdit*>("lcdInput");
        QVERIFY(input);
        click(input);
        input->clear();
        QTest::keyClicks(input, "2+3*4");
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(m_app->calculator->findChild<QLabel*>("lcdResult")->text(), QString("14"));

        input->clear();
        // on-screen keypad (same theme): 7 + 8 = 15
        for (const char* key : {"7", "+", "8"}) click(findButton(m_app->calculator, key));
        QCOMPARE(input->text(), QString("7+8"));
        click(findButton(m_app->calculator, "="));
        QCOMPARE(m_app->calculator->findChild<QLabel*>("lcdResult")->text(), QString("15"));
        click(findButton(m_app->calculator, "AC"));
        QVERIFY(input->text().isEmpty());
        click(findButton(m_app->calculator, "sin"));
        QVERIFY(input->text().startsWith("sin("));
        click(findButton(m_app->calculator, "DEL"));
        QVERIFY(!input->text().endsWith("("));
    }

    // ------------------------------------------------------------------ formula solver, constants
    void formulaSolverSolvesFromTheUi() {
        goTo(m_app->solver);
        m_app->solver->selectFormula("snell_law");
        auto edits = m_app->solver->findChildren<QLineEdit*>("lcdInput");
        QVERIFY(edits.size() >= 4);
        edits[0]->setText("1"); edits[1]->setText("30"); edits[2]->setText("1.5"); edits[3]->clear();
        click(findButton(m_app->solver, "Solve"));
        const QString all = allLabelTexts(m_app->solver).join("|");
        QVERIFY2(all.contains("Solved"), "Solve showed no success status");
    }
    void constantsCopyButtonCopiesTheValue() {
        goTo(m_app->constants);
        auto* table = m_app->constants->findChild<QTableWidget*>();
        QVERIFY(table);
        table->selectRow(0);
        click(findButton(m_app->constants, "Copy value"));
        QCOMPARE(QApplication::clipboard()->text(), QString("299792458"));
    }

    // ------------------------------------------------------------------ keyboard
    void virtualKeyboardInsertsIntoTheFocusedField() {
        goTo(m_app->solver);
        click(win().keyboardButton());
        QVERIFY(win().keyboard()->isVisible());
        auto edits = m_app->solver->findChildren<QLineEdit*>("lcdInput");
        click(edits[0]);
        edits[0]->clear();
        click(findButton(win().keyboard(), "π"));
        QCOMPARE(edits[0]->text(), QString("π"));
        click(findButton(win().keyboard(), "sin("));
        QCOMPARE(edits[0]->text(), QString("πsin("));
        click(win().keyboardButton());
        QVERIFY(!win().keyboard()->isVisible());
    }
    void keyboardWorksEvenBeforeAnyFieldWasClicked() {
        goTo(m_app->calculator);
        auto* input = m_app->calculator->findChild<QLineEdit*>("lcdInput");
        input->clearFocus();
        input->clear();
        click(win().keyboardButton());
        click(findButton(win().keyboard(), "π"));
        QVERIFY2(!input->text().isEmpty(), "keyboard pressed with no focused field inserted nothing");
        click(win().keyboardButton());
    }

    // ------------------------------------------------------------------ study workspace (Markdown notes)
    MarkdownWorkspace* workspace() { goTo(m_app->notes); return m_app->notes->findChild<MarkdownWorkspace*>(); }
    QToolButton* tool(MarkdownWorkspace* w, const QString& text) {
        for (auto* b : w->findChildren<QToolButton*>()) if (b->text() == text && b->isVisibleTo(w->window())) return b;
        return nullptr;
    }
    void clickTool(MarkdownWorkspace* w, const QString& text) {
        QToolButton* b = tool(w, text);
        QVERIFY2(b, qPrintable("toolbar button not found: " + text));
        click(b);
    }
    void workspaceHasSnippetToolbar() {
        auto* w = workspace();
        QVERIFY(w);
        w->setMode(MarkdownWorkspace::Split);
        for (const char* label : {"θ", "λ", "δ", "n₁", "n₂", "sin", "arcsin", "$ $", "H1", "H2", "H3", "H4 ⚠", "•", "Problem template", "Formulas ▾"})
            QVERIFY2(tool(w, label), label);
    }
    void snippetButtonsInsertAtTheCursor() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        clickTool(w, "θ");
        clickTool(w, "n₁");
        QCOMPARE(w->markdown(), QString("θn₁"));
        w->setMarkdown("");
        clickTool(w, "sin");
        QCOMPARE(w->markdown(), QString("sin()"));
        QCOMPARE(w->editor()->textCursor().position(), 4);                  // cursor between the brackets
        typeText(w->editor(), QString::fromUtf8("θ"));
        QCOMPARE(w->markdown(), QString("sin(θ)"));
        w->setMarkdown("n1 sin t1");
        QTextCursor c = w->editor()->textCursor();
        c.select(QTextCursor::Document);
        w->editor()->setTextCursor(c);
        clickTool(w, "$ $");
        QCOMPARE(w->markdown(), QString("$n1 sin t1$"));                    // wraps the selection
        clickTool(w, "arcsin");
        QVERIFY(w->markdown().contains("arcsin("));
    }
    void headingButtonsSetAndToggleTheLevel() {
        auto* w = workspace();
        w->setMarkdown("Snell's law");
        w->editor()->setFocus();
        clickTool(w, "H3");
        QCOMPARE(w->markdown(), QString("### Snell's law"));
        clickTool(w, "H1");
        QCOMPARE(w->markdown(), QString("# Snell's law"));
        clickTool(w, "H1");
        QCOMPARE(w->markdown(), QString("Snell's law"));                    // same level again removes it
        clickTool(w, "H4 ⚠");
        QCOMPARE(w->markdown(), QString("#### Snell's law"));
        clickTool(w, "•");
        clickTool(w, "•");
    }
    void problemTemplateInsertsTheThreeSections() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        clickTool(w, "Problem template");
        const QString text = w->markdown();
        QVERIFY(text.contains("### Given:") && text.contains("### Formula:") && text.contains("### Substitution & Solution:"));
        QVERIFY(text.indexOf("### Given:") < text.indexOf("### Formula:") && text.indexOf("### Formula:") < text.indexOf("### Substitution & Solution:"));
        QCOMPARE(w->editor()->textCursor().block().text(), QString("- "));        // ready to type the first given value
        typeText(w->editor(), QString::fromUtf8("n₁ = 1.5"));
        QVERIFY(w->markdown().contains("- n₁ = 1.5"));
        click(tool(w, "Problem template"));                                       // a second one starts on its own line
        QCOMPARE(w->markdown().count("### Given:"), 2);
    }
    void enterContinuesAndEndsBulletLists() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        typeText(w->editor(), QString::fromUtf8("- first"));
        QTest::keyClick(w->editor(), Qt::Key_Return);
        QCOMPARE(w->markdown(), QString("- first\n- "));
        typeText(w->editor(), QString::fromUtf8("second"));
        QTest::keyClick(w->editor(), Qt::Key_Return);
        QTest::keyClick(w->editor(), Qt::Key_Return);                             // empty item ends the list
        QCOMPARE(w->markdown(), QString("- first\n- second\n"));
    }
    void editorIsColourCoded() {
        auto* w = workspace();
        w->setMarkdown("# Title\n## Chapter\n### Section\n#### Warning\n- item with $x$\n");
        QApplication::processEvents();
        auto* doc = w->editor()->document();
        auto colorOf = [&](int block, int column = 0) {
            const auto formats = doc->findBlockByNumber(block).layout()->formats();
            for (const auto& r : formats) if (r.start <= column && column < r.start + r.length) return r.format.foreground().color();
            return QColor();
        };
        QCOMPARE(colorOf(0), QColor("#d79921"));
        QCOMPARE(colorOf(1), QColor("#e78a4e"));
        QCOMPARE(colorOf(2), QColor("#a9b665"));
        QCOMPARE(colorOf(3), QColor("#ea696c"));
        QCOMPARE(colorOf(4, 2), QColor("#ebdbb2"));                                  // bullet text
        QCOMPARE(colorOf(4, 14), QColor("#83a598"));                                 // the formula inside the bullet
    }
    void previewShowsHeadingsAndFormulas() {
        auto* w = workspace();
        w->setMarkdown("# Optics notes\n\n- see $n₁ sin θ₁ = n₂ sin θ₂$\n");
        click(findButton(w, "Preview"));
        QVERIFY(w->preview()->isVisible() && !w->editor()->isVisible());
        QTextBlock heading = w->preview()->document()->begin();
        QCOMPARE(heading.blockFormat().headingLevel(), 1);
        QCOMPARE(heading.begin().fragment().charFormat().foreground().color(), QColor("#d79921"));
        QCOMPARE(heading.begin().fragment().charFormat().fontPointSize(), 18.0);
        bool formulaImage = false;                                                   // the formula is a LaTeX image, not code text
        for (QTextBlock b = w->preview()->document()->begin(); b.isValid(); b = b.next())
            for (auto it = b.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().isImageFormat()) {
                    const QString name = it.fragment().charFormat().toImageFormat().name();
                    const QVariant res = w->preview()->document()->resource(QTextDocument::ImageResource, QUrl(name));
                    formulaImage = formulaImage || (res.isValid() && !res.value<QPixmap>().isNull());
                }
        QVERIFY2(formulaImage, "formula in the preview is not rendered as a LaTeX image");
        click(findButton(w, "Split"));
        QVERIFY(w->preview()->isVisible() && w->editor()->isVisible());
        click(findButton(w, "Edit"));
        QVERIFY(w->editor()->isVisible() && !w->preview()->isVisible());
        click(findButton(w, "Split"));
    }
    void previewKeepsImagesWithoutAltText() {
        auto* w = workspace();
        w->setMode(MarkdownWorkspace::Preview);
        w->setMarkdown("![](file:///tmp/phy104-diagram.png)");
        int images = 0;                                                       // Qt drops ![](...), so the preview must rescue it
        for (QTextBlock b = w->preview()->document()->begin(); b.isValid(); b = b.next())
            for (auto it = b.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().isImageFormat()) ++images;
        QCOMPARE(images, 1);
        w->setMarkdown("```\n![](not-an-image-in-a-fence.png)\n```");
        int fenced = 0;                                                       // code blocks keep their text untouched
        for (QTextBlock b = w->preview()->document()->begin(); b.isValid(); b = b.next())
            for (auto it = b.begin(); !it.atEnd(); ++it)
                if (it.fragment().charFormat().isImageFormat()) ++fenced;
        QCOMPARE(fenced, 0);
        QVERIFY(w->preview()->toPlainText().contains("![](not-an-image-in-a-fence.png)"));
        w->setMarkdown("");
        w->setMode(MarkdownWorkspace::Split);
    }
    void toolbarFromPreviewModeSwitchesToSplit() {
        auto* w = workspace();
        w->setMode(MarkdownWorkspace::Preview);
        clickTool(w, "θ");
        QCOMPARE(w->mode(), MarkdownWorkspace::Split);
        QVERIFY(w->markdown().contains("θ"));
        w->setMarkdown("");
    }

    // ------------------------------------------------------------------ notes <-> session snapshot <-> .phy file
    void typingAutosavesIntoTheSessionAndEverySnapshot() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        QSignalSpy spy(w, &MarkdownWorkspace::contentChanged);
        typeText(w->editor(), QString::fromUtf8("# Study notes"));
        QCOMPARE(spy.count(), 0);                                                   // debounced: nothing written per keystroke
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 2500);
        QCOMPARE(QString::fromStdString(m_app->context().log.notes()), QString("# Study notes"));
        for (const char* name : {"Refraction & TIR", "Lenses & Mirrors", "Prisms", "Wave Interference"}) {
            const Snapshot* snap = m_app->context().log.snapshot(name);
            QVERIFY2(snap, name);
            QCOMPARE(QString::fromStdString(snap->notesContent), QString("# Study notes"));
        }
    }
    void loadingTextDoesNotTriggerAnAutosave() {
        auto* w = workspace();
        QSignalSpy spy(w, &MarkdownWorkspace::contentChanged);
        w->setMarkdown("## loaded, not typed");
        QTest::qWait(900);
        QCOMPARE(spy.count(), 0);
    }
    void flushSavesImmediately() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        typeText(w->editor(), QString::fromUtf8("last edit before quitting"));
        w->flush();                                                                 // what aboutToQuit does
        QCOMPARE(QString::fromStdString(m_app->context().log.notes()), QString("last edit before quitting"));
    }
    void notesSurviveARestartTogetherWithTheSnapshots() {
        auto* w = workspace();
        w->setMarkdown("");
        w->editor()->setFocus();
        typeText(w->editor(), QString::fromUtf8("### Snell's law"));
        QTest::keyClick(w->editor(), Qt::Key_Return);
        typeText(w->editor(), QString::fromUtf8("- $n₁ sin θ₁ = n₂ sin θ₂$"));
        w->flush();
        click(findButton(m_app->notes, "Save session"));                           // the button of the page that is showing
        QVERIFY(QFile::exists(m_tmp.path() + "/session.phy"));

        App second(App::Paths{DATA_DIR, m_tmp.path() + "/session.phy"});           // a fresh start reading the same file
        auto* w2 = second.notes->findChild<MarkdownWorkspace*>();
        QCOMPARE(w2->markdown(), QString("### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$"));
        const Snapshot* snap = second.context().log.snapshot("Refraction & TIR");
        QVERIFY(snap && !snap->registers.empty() && !snap->parameters.empty());      // parameters + calculator registers are in the snapshot
        QCOMPARE(QString::fromStdString(snap->notesContent), w2->markdown());
        QVERIFY(!snap->headline.empty());
    }


    // ------------------------------------------------------------------ the six screens of the second Figma batch
    QString calcText() { return "Calculate &&\nSimulate"; }
    void newScreensShowTheNumbersOfTheDesign() {
        struct Case { QWidget* page; const char* big; };
        const Case cases[] = {{m_app->newton, "1.5492 mm"}, {m_app->thinFilm, "1.0000"}, {m_app->grating, "+17.4576°"},
                              {m_app->singleSlit, "0.2865°"}, {m_app->polarization, "12.5000 W/m²"}, {m_app->michelson, "+2.3704"}};
        for (const Case& c : cases) {
            goTo(c.page);
            QTRY_COMPARE_WITH_TIMEOUT(bigValue(c.page, 0), QString::fromUtf8(c.big), 1500);
        }
        goTo(m_app->grating);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating, 1), QString("7 orders"), 1500);
        goTo(m_app->singleSlit);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->singleSlit, 1), QString("10.0001 mm"), 1500);
        goTo(m_app->newton);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->newton, 1), QString("3.0984 mm"), 1500);
    }
    void secondTabsAndDropdownsDriveTheDiagram() {
        // --- thin film: the Transmission tab flips to the complementary intensity and re-captions both panels
        goTo(m_app->thinFilm);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->thinFilm), QString("1.0000"), 1500);
        click(modeTab(m_app->thinFilm, "Transmission"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->thinFilm), QString("0.0000"), 1500);          // t = 100 nm is dark in transmission
        QVERIFY2(allLabelTexts(m_app->thinFilm).contains("Film layers / transmitted beam"), "diagram title did not follow the tab");
        QVERIFY(allLabelTexts(m_app->thinFilm).contains("Transmitted intensity vs thickness"));
        click(modeTab(m_app->thinFilm, "Reflection"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->thinFilm), QString("1.0000"), 1500);
        QVERIFY(allLabelTexts(m_app->thinFilm).contains("Film layers / two reflected rays"));

        // --- Michelson: the Fringes tab shows the pattern numbers instead of the displacement numbers
        goTo(m_app->michelson);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson), QString("+2.3704"), 1500);
        click(modeTab(m_app->michelson, "Fringes"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson), QString("316.4000 nm"), 1500);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson, 1), QString("12.6422 fringes"), 1500);
        QVERIFY2(allLabelTexts(m_app->michelson).contains("Optical bench / fringe pattern"), "bench title did not follow the tab");
        // beam balance sets the contrast, not the fringe spacing
        chooseItem(comboWith(m_app->michelson, "Unequal beams · V = 0.60"), 1);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson), QString("316.4000 nm"), 1500);
        QTRY_VERIFY_WITH_TIMEOUT(allLabelTexts(m_app->michelson).join(QStringLiteral(" ")).contains("contrast V = 0.60"), 1500);
        // reference-arm dropdown: M1 on a piezo, and the diagram says so
        chooseItem(comboWith(m_app->michelson, "M1 on a piezo (static)"), 1);
        QTRY_VERIFY_WITH_TIMEOUT(allLabelTexts(m_app->michelson).contains("Optical bench / piezo scan of M1"), 1500);
        click(modeTab(m_app->michelson, "Displacement"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson), QString("+2.3704"), 1500);
        click(findButton(m_app->michelson, "Reset parameters"));                                  // back to the defaults for later tests
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->michelson), QString("+2.3704"), 1500);

        // --- Newton's rings: the viewing-mode dropdown now reaches the physics and the diagram
        goTo(m_app->newton);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->newton), QString("1.5492 mm"), 1500);
        chooseItem(comboWith(m_app->newton, "Transmitted light"), 1);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->newton), QString("1.6432 mm"), 1500);
        QVERIFY(allLabelTexts(m_app->newton).contains("Lens on plate / transmitted rings"));
        chooseItem(comboWith(m_app->newton, "Reflected light"), 0);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->newton), QString("1.5492 mm"), 1500);
    }
    void remainingSecondTabsShowTheirOwnNumbers() {
        // grating: the Orders tab lists every peak instead of one selected order
        goTo(m_app->grating);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating), QString("+17.4576°"), 1500);
        click(modeTab(m_app->grating, "Orders"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating), QString("7 orders"), 1500);
        QVERIFY2(allLabelTexts(m_app->grating).contains("Grating / every propagating order"), "orders title missing");
        click(modeTab(m_app->grating, "Transmission"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating), QString("+17.4576°"), 1500);

        // single slit: Geometry puts the minima on the screen
        goTo(m_app->singleSlit);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->singleSlit), QString("0.2865°"), 1500);
        click(modeTab(m_app->singleSlit, "Geometry"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->singleSlit), QString("5.0001 mm"), 1500);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->singleSlit, 1), QString("7.1501 mm"), 1500);
        QVERIFY(allLabelTexts(m_app->singleSlit).contains("Slit / where the minima land"));
        click(modeTab(m_app->singleSlit, "Intensity"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->singleSlit), QString("0.2865°"), 1500);

        // polarization: the Brewster tab replaces Malus' law with the Fresnel reflectances
        goTo(m_app->polarization);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->polarization), QString("12.5000 W/m²"), 1500);
        click(modeTab(m_app->polarization, "Brewster"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->polarization), QString("56.3099°"), 1500);
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->polarization, 1), QString("0.1479"), 1500);
        QVERIFY(allLabelTexts(m_app->polarization).contains("Interface at Brewster / reflected beam s-polarized"));
        auto fields = m_app->polarization->findChildren<UnitField*>();                       // Iᵢₙ, θ, n₁, n₂
        QCOMPARE(fields.size(), 4);
        typeInto(fields[3], "1.333");                                                        // water instead of glass
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->polarization), QString("53.1232°"), 1500);
        click(findButton(m_app->polarization, "Reset parameters"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->polarization), QString("12.5000 W/m²"), 1500);
    }
    void newScreensHaveTheSaveExportHeader() {
        for (QWidget* page : QList<QWidget*>{m_app->newton, m_app->michelson, m_app->thinFilm, m_app->grating, m_app->singleSlit, m_app->polarization}) {
            goTo(page);
            QVERIFY2(findButton(page, "Save"), "missing Save");
            QVERIFY2(!findButton(page, "Save session"), "old header still shown");
            auto* exportButton = page->findChild<QToolButton*>("exportMenu");
            QVERIFY2(exportButton && exportButton->menu() && exportButton->menu()->actions().size() == 3,
                     "missing the export icon with PDF / Excel / CSV");
        }
    }
    void newScreensCalculateAndSendToNotes() {
        goTo(m_app->newton);
        const int runs = m_app->context().log.savedCalculations();
        click(findButton(m_app->newton, calcText()));
        QCOMPARE(m_app->context().log.savedCalculations(), runs + 1);
        QCOMPARE(QString::fromStdString(m_app->context().log.runs().back().experiment), QString("Newton's Rings"));
        auto* w = workspace();
        w->setMarkdown("");
        m_app->context().setNotes("");
        goTo(m_app->newton);
        click(findButton(m_app->newton, "Send to\nNotes"));
        const QString notes = QString::fromStdString(m_app->context().log.notes());
        QVERIFY2(notes.contains("### Newton's Rings"), qPrintable(notes));
        QVERIFY(notes.contains("1.5492 mm") && notes.contains("λ = 600 nm"));
        QCOMPARE(workspace()->markdown(), notes);                                  // the notes page reloaded the appended text
    }
    void newScreensPauseOnInvalidInputAndRestore() {
        goTo(m_app->newton);
        auto fields = m_app->newton->findChildren<UnitField*>();                    // λ, R, m
        typeInto(fields[2], "2.5");                                                  // order must be an integer
        QTRY_VERIFY_WITH_TIMEOUT(!findButton(m_app->newton, calcText())->isEnabled(), 1500);
        QVERIFY2(findButton(m_app->newton, "Restore m = 4"), "no Restore button");
        QVERIFY(m_app->newton->isPaused());
        QCOMPARE(win().engineLabel()->text(), QString("● CALCULATION PAUSED"));
        click(findButton(m_app->newton, "Restore m = 4"));
        QTRY_VERIFY_WITH_TIMEOUT(findButton(m_app->newton, calcText())->isEnabled(), 1500);
        QCOMPARE(fields[2]->text(), QString("4"));
        QCOMPARE(win().engineLabel()->text(), QString("● ENGINE READY"));

        goTo(m_app->grating);
        auto g = m_app->grating->findChildren<UnitField*>();                        // N, λ, m, M
        typeInto(g[2], "4");                                                         // order 4 has no real angle
        QTRY_VERIFY_WITH_TIMEOUT(m_app->grating->isPaused(), 1500);
        click(findButton(m_app->grating, "Restore m = +1"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating, 0), QString("+17.4576°"), 1500);
    }
    void gratingSpinnerSteps() {
        goTo(m_app->grating);
        auto g = m_app->grating->findChildren<UnitField*>();
        click(g[2]->findChild<QWidget*>("spinUp"));
        QCOMPARE(g[2]->text(), QString("+2"));
        QTRY_COMPARE_WITH_TIMEOUT(bigValue(m_app->grating, 0), QString("+36.8699°"), 1500);
        click(g[2]->findChild<QWidget*>("spinDown"));
        click(m_app->grating->findChild<QPushButton*>());                            // harmless: any button
    }

    // ------------------------------------------------------------------ scientific keyboard + Equation Lab
    QPushButton* sciKey(ScientificKeyboard* k, const QString& legend) {
        for (auto* b : k->findChildren<QPushButton*>())
            if (b->property("legend").toString() == legend && b->isVisibleTo(k->window())) return b;
        return nullptr;
    }
    void equationLabEvaluatesTheDesignExample() {
        goTo(m_app->equationLab);
        auto* lab = m_app->equationLab;
        lab->source()->setPlainText("r_m = \\sqrt{\\frac{m\\lambda R}{n}}");
        QTRY_COMPARE_WITH_TIMEOUT(lab->resultText(), QString("r_m = 1.5492 mm"), 2000);
        QCOMPARE(lab->variablesTable()->rowCount(), 5);                              // r_m, m, lambda, R, n
        auto* m = lab->variablesTable()->item(1, 1);
        QCOMPARE(lab->variablesTable()->item(1, 0)->text(), QString("m"));
        m->setText("9");                                                              // edit a known value
        QTRY_COMPARE_WITH_TIMEOUT(lab->resultText(), QString("r_m = 2.3238 mm"), 2000);
        lab->source()->setPlainText("\\frac{1}{s} + \\frac{1}{s'} = \\frac{1}{f}");
        QTRY_VERIFY_WITH_TIMEOUT(lab->parsed(), 2000);
        lab->source()->setPlainText("\\frac{1}");
        QTRY_VERIFY_WITH_TIMEOUT(!lab->parsed(), 2000);                              // syntax error is reported, not a crash
    }
    void scientificKeyboardWorksLikeTheDesign() {
        goTo(m_app->equationLab);
        auto* lab = m_app->equationLab;
        auto* kb = lab->keyboard();
        QToolButton* toggle = nullptr;
        for (auto* b : lab->findChildren<QToolButton*>("fmtAccent")) toggle = b;
        QVERIFY(toggle);
        click(toggle);
        QVERIFY(kb->isVisible());
        lab->source()->setPlainText("");
        QApplication::processEvents();
        click(sciKey(kb, "λ"));                                                       // Greek page is first
        QCOMPARE(kb->pending(), QString("\\lambda"));
        QCOMPARE(lab->source()->toPlainText(), QString());                            // nothing is inserted until INSERT
        click(findButton(kb, "INSERT / ENTER  ↵"));
        QCOMPARE(lab->source()->toPlainText(), QString("\\lambda"));
        QVERIFY(kb->pending().isEmpty());
        QVERIFY2(lab->source()->hasFocus(), "focus must stay in the source editor");

        click(findButton(kb, "Math operators"));                                      // braces place the caret inside
        click(sciKey(kb, "√"));
        click(findButton(kb, "INSERT / ENTER  ↵"));
        QCOMPARE(lab->source()->toPlainText(), QString("\\lambda\\sqrt{}"));
        QCOMPARE(lab->source()->textCursor().position(), lab->source()->toPlainText().size() - 1);
        QTest::keyClick(lab->source(), 'x');
        QCOMPARE(lab->source()->toPlainText(), QString("\\lambda\\sqrt{x}"));
        click(sciKey(kb, "/"));                                                       // fraction: caret in the numerator
        QCOMPARE(kb->pending(), QString("\\frac{}{}"));
        click(findButton(kb, "AC"));
        QVERIFY2(kb->pending().isEmpty() && lab->source()->toPlainText() == "\\lambda\\sqrt{x}", "AC clears the buffer, not the equation");

        click(sciKey(kb, "2"));
        click(sciKey(kb, "x²"));
        QCOMPARE(kb->pending(), QString("2^{2}"));
        click(findButton(kb, "⌫"));                                                   // removes the last pending token
        QCOMPARE(kb->pending(), QString("2"));
        click(findButton(kb, "⌫"));
        QVERIFY(kb->pending().isEmpty());
        const int before = lab->source()->toPlainText().size();
        click(findButton(kb, "⌫"));                                                   // empty buffer: deletes in the source
        QCOMPARE(lab->source()->toPlainText().size(), before - 1);

        const int pos = lab->source()->textCursor().position();
        click(findButton(kb, "←"));
        QCOMPARE(lab->source()->textCursor().position(), pos - 1);
        click(findButton(kb, "→"));
        QCOMPARE(lab->source()->textCursor().position(), pos);

        click(findButton(kb, "Greek symbols"));                                       // SHIFT latches capital Greek
        click(findButton(kb, "SHIFT"));
        QVERIFY(kb->shiftLatched());
        QVERIFY2(sciKey(kb, "δ") && sciKey(kb, "δ")->text() == "Δ", "SHIFT shows capital legends");
        click(sciKey(kb, "δ"));
        QCOMPARE(kb->pending(), QString("\\Delta"));
        click(findButton(kb, "SHIFT"));
        click(findButton(kb, "AC"));

        click(findButton(kb, "Functions"));
        click(sciKey(kb, "sin"));
        QCOMPARE(kb->pending(), QString("\\sin()"));
        click(findButton(kb, "AC"));
        click(findButton(kb, "Variables"));                                            // all variables of variables.json
        int variableKeys = 0;
        for (auto* b : kb->findChildren<QPushButton*>("rkey")) if (!b->icon().isNull()) ++variableKeys;
        QVERIFY2(variableKeys >= 55, qPrintable(QString::number(variableKeys)));

        kb->setFocus();
        QTest::keyClick(kb, Qt::Key_Escape);                                           // Esc closes without inserting
        QVERIFY(!kb->isVisible() && !toggle->isChecked());
    }

    // ------------------------------------------------------------------ the export icon of the header (PDF / Excel / CSV)
    void exportIconOpensTheChosenFormat() {
        goTo(m_app->prisms);
        auto* button = m_app->prisms->findChild<QToolButton*>("exportMenu");
        QVERIFY2(button && button->menu(), "the header has no export icon");
        auto actions = button->menu()->actions();
        QCOMPARE(actions.size(), 3);
        QCOMPARE(actions[0]->text(), QString("PDF report…"));
        QCOMPARE(actions[1]->text(), QString("Excel (.xlsx)…"));
        QCOMPARE(actions[2]->text(), QString("CSV (.csv)…"));

        actions[1]->trigger();                                              // Excel
        QCOMPARE(win().currentPage(), idx(m_app->exportPage));
        QVERIFY(findButton(m_app->exportPage, "Export workbook"));
        goTo(m_app->prisms);
        actions[2]->trigger();                                              // CSV
        QCOMPARE(win().currentPage(), idx(m_app->exportPage));
        QVERIFY(findButton(m_app->exportPage, "Export CSV"));               // the format was preselected
        goTo(m_app->prisms);
        actions[0]->trigger();                                              // PDF report
        QCOMPARE(win().currentPage(), idx(m_app->reportPage));
    }
    void notesPageIsOnlyTheMarkdownWorkspace() {
        goTo(m_app->notes);
        QVERIFY(m_app->notes->findChild<MarkdownWorkspace*>());
        QVERIFY2(!findButton(m_app->notes, "Export workbook"), "the export GUI is still on the notes page");
        QStringList texts;
        for (auto* l : m_app->notes->findChildren<QLabel*>()) texts << l->text();
        QVERIFY2(!texts.contains("Formula reference"), "the formula reference is still on the notes page");
        QVERIFY2(!texts.contains("Export preview"), "the export preview is still on the notes page");
    }

    // ------------------------------------------------------------------ dedicated Export (Excel/CSV) page
    const Snapshot* selectedSeries() {
        auto* list = m_app->exportPage->seriesList();
        if (!list || !list->currentItem()) return nullptr;
        return m_app->context().log.snapshot(list->currentItem()->data(Qt::UserRole).toString().toStdString());
    }
    void exportPageListsEverySeriesAndDescribesIt() {
        goTo(m_app->exportPage);
        auto* list = m_app->exportPage->seriesList();
        QVERIFY(list && list->count() > 0);
        int withRows = 0;                                                       // every published series that has rows
        for (const auto& entry : m_app->context().log.snapshots()) if (!entry.second.series.rows.empty()) ++withRows;
        QCOMPARE(list->count(), withRows);
        for (int i = 0; i < list->count(); ++i) {
            list->setCurrentRow(i);
            QApplication::processEvents();
            const Snapshot* snap = selectedSeries();
            QVERIFY2(snap && !snap->series.rows.empty(), "series without data in the list");
            const QStringList texts = allLabelTexts(m_app->exportPage);
            QVERIFY2(texts.contains(QString::fromStdString(snap->series.name)), "series name not described");
            QVERIFY2(texts.contains(QString::fromStdString(snap->series.description)), "series description missing");
        }
    }
    void exportPageShowsEveryRowOfTheChosenSeries() {
        goTo(m_app->exportPage);
        auto* list = m_app->exportPage->seriesList();
        QVERIFY(list && list->count() > 0);
        int mostRows = 0;
        for (int i = 0; i < list->count(); ++i) {
            list->setCurrentRow(i);
            QApplication::processEvents();
            const Snapshot* snap = selectedSeries();
            QVERIFY(snap);
            QCOMPARE(m_app->exportPage->previewTable()->rowCount(), static_cast<int>(snap->series.rows.size()));
            QCOMPARE(m_app->exportPage->previewTable()->columnCount(), static_cast<int>(snap->series.headers.size()));
            mostRows = std::max(mostRows, m_app->exportPage->previewTable()->rowCount());
        }
        QVERIFY2(mostRows > 7, "the dedicated page must show every row, not the 7-row sample of the Notes page");
    }
    void exportPageWritesBothFormatsAndRemembersTheFile() {
        goTo(m_app->exportPage);
        const Snapshot* snap = selectedSeries();
        QVERIFY(snap);
        const QString base = "PHY104_" + QString::fromStdString(snap->series.id).section('_', 0, 0);
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104";
        QDir(dir).removeRecursively();

        click(findButton(m_app->exportPage, "Export workbook"));
        QVERIFY2(QFile::exists(dir + "/" + base + ".xlsx"), qPrintable("no workbook in " + dir));
        QCOMPARE(QString::fromStdString(m_app->context().log.lastExport().path), dir + "/" + base + ".xlsx");
        QCOMPARE(QString::fromStdString(m_app->context().log.lastExport().format), QString("Excel (.xlsx)"));
        QVERIFY(allLabelTexts(m_app->exportPage).contains(base + ".xlsx"));      // the recent-export card

        click(findButton(m_app->exportPage, "Export as CSV instead"));
        QVERIFY2(QFile::exists(dir + "/" + base + ".csv"), qPrintable("no CSV in " + dir));
        QCOMPARE(QString::fromStdString(m_app->context().log.lastExport().format), QString("CSV (.csv)"));
        QVERIFY(allLabelTexts(m_app->exportPage).contains(base + ".csv"));
    }
    void recentExportSurvivesARestart() {
        QVERIFY(m_app->context().log.hasExport());                              // written by the test above
        const QString path = QString::fromStdString(m_app->context().log.lastExport().path);
        App second(App::Paths{DATA_DIR, m_tmp.path() + "/session.phy"});
        QVERIFY(second.context().log.hasExport());
        QCOMPARE(QString::fromStdString(second.context().log.lastExport().path), path);
        QStringList texts;
        for (auto* l : second.exportPage->findChildren<QLabel*>()) texts << l->text();
        QVERIFY2(texts.contains(QFileInfo(path).fileName()), "the recent-export card forgot the file");
    }

    // ------------------------------------------------------------------ the printable report
    void reportPageCapturesEveryScreenAndWritesAPdf() {
        goTo(m_app->reportPage);
        auto* page = m_app->reportPage;
        QTRY_VERIFY_WITH_TIMEOUT(page->sectionCount() >= 6, 2000);
        QVERIFY(page->html().contains("PHY104 Laboratory Report"));
        QVERIFY2(page->html().contains("img://diagram-"), "the diagrams were not captured");
        QVERIFY(page->html().contains("img://chart-"));
        QVERIFY(page->html().contains("Thin-Film Interference"));

        // a section that is unticked leaves the document immediately
        QCheckBox* diagrams = nullptr;
        for (auto* c : page->findChildren<QCheckBox*>())
            if (c->text().startsWith("Diagrams")) diagrams = c;
        QVERIFY2(diagrams, "no diagrams checkbox");
        click(diagrams);
        QVERIFY(!page->html().contains("img://diagram-"));
        click(diagrams);
        QVERIFY(page->html().contains("img://diagram-"));

        // the file really is a PDF, and the recent-export card remembers it
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104";
        QDir(dir).removeRecursively();
        click(page->saveButton());
        const QString path = dir + "/PHY104_report_" + QDate::currentDate().toString("yyyy-MM-dd") + ".pdf";
        QVERIFY2(QFile::exists(path), qPrintable("no PDF at " + path));
        QVERIFY(QFileInfo(path).size() > 2000);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.read(5), QByteArray("%PDF-"));
        file.close();
        QCOMPARE(QString::fromStdString(m_app->context().log.lastExport().format), QString("PDF report"));
        QVERIFY(allLabelTexts(page).contains(QFileInfo(path).fileName()));
    }
};

QTEST_MAIN(InteractionTest)
#include "test_interaction.moc"
