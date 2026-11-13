#include "NotesExportPage.h"
#include "AppContext.h"
#include "Components.h"
#include "DataExport.h"
#include "Icons.h"
#include "LatexRenderer.h"
#include "MarkdownWorkspace.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextList>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {
namespace {
const char* const kExperiments[4] = {"Refraction & TIR", "Lenses & Mirrors", "Prisms", "Wave Interference"};
const char* const kStarterNotes =
    "# Lab notes\n\n## Refraction & TIR\n\n### Snell's law\n- $n₁ sin θ₁ = n₂ sin θ₂$\n- compare the fitted index with the reference value\n\n"
    "#### Exam warning\nAngles are measured from the normal, not from the surface.\n";
}

NotesExportPage::NotesExportPage(AppContext* ctx, QWidget* parent)
    : BasePage("Formulas, Notes & Export", "LAB NOTEBOOK / Reference, annotate and prepare your report data", ctx, parent) {
    auto cols = addThreeColumns(250, 262);
    const Theme& th = ThemeManager::instance().theme();

    // ---------------------------------------------------------------- formula reference
    auto* ref = new Panel("Formula reference", QString::number(ctx->topics.size()).rightJustified(2, '0') + " TOPICS");
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    auto* il = new QVBoxLayout(inner);
    il->setContentsMargins(0, 0, 6, 0);
    il->setSpacing(6);
    for (std::size_t i = 0; i < ctx->topics.size(); ++i) {
        const ReferenceTopic& t = ctx->topics[i];
        auto* head = new QLabel(QString::fromStdString(t.title));
        head->setObjectName("sectionTitle");
        il->addWidget(head);
        for (const auto& eq : t.equations) {
            auto* e = new LatexWidget;                                  // formulas are typeset with the LaTeX engine
            e->setPointSize(11);
            e->setAlignLeft(true);
            e->setLatex(LatexRenderer::fromPlainMath(QString::fromStdString(eq)));
            il->addWidget(e);
        }
        auto* note = new QLabel(QString::fromStdString(t.note));
        note->setObjectName("hint");
        note->setWordWrap(true);
        il->addWidget(note);
        if (i + 1 < ctx->topics.size()) {
            auto* line = new QFrame;
            line->setFixedHeight(1);
            line->setStyleSheet(QString("background: %1;").arg(th.border.name()));
            il->addSpacing(4);
            il->addWidget(line);
            il->addSpacing(4);
        }
    }
    auto* discipline = new NoticeCard;
    const QString du = QString::fromStdString(ctx->unitDiscipline);
    discipline->setNotice(du.section('\n', 0, 0), du.section('\n', 1), false);
    il->addSpacing(8);
    il->addWidget(discipline);
    il->addStretch();
    scroll->setWidget(inner);
    ref->body()->addWidget(scroll);
    cols.left->addWidget(ref, 1);

    // ---------------------------------------------------------------- session notes: the Markdown study workspace
    m_notesPanel = new Panel("Session notes", "SAVED " + ctx->lastSavedTime());
    m_notes = new MarkdownWorkspace;
    const QString saved = QString::fromStdString(ctx->log.notes());
    m_notes->setMarkdown(saved.isEmpty() ? QString::fromUtf8(kStarterNotes) : saved);
    m_notes->setMinimumHeight(400);
    m_notesPanel->body()->addWidget(m_notes, 1);
    m_linked = new QLabel;
    m_linked->setObjectName("caption");
    m_notesPanel->body()->addWidget(m_linked);
    cols.center->addWidget(m_notesPanel, 7);

    // autosave: the workspace reports its content ~0.7 s after the last keystroke; the context stores it in the
    // session log (and into every snapshot's notesContent) and writes the .phy file
    connect(m_notes, &MarkdownWorkspace::contentChanged, ctx, &AppContext::setNotes);
    connect(ctx, &AppContext::notesAppended, this, [this] { m_notes->setMarkdown(QString::fromStdString(m_ctx->log.notes())); });
    connect(ctx, &AppContext::notesSaved, this, [this] {
        m_notesPanel->setMeta("SAVED " + m_ctx->lastSavedTime());
        m_notes->setSavedText("Saved " + m_ctx->lastSavedTime());
    });

    // ---------------------------------------------------------------- export preview
    m_previewPanel = new Panel("Export preview", "");
    m_table = new QTableWidget;
    m_table->setObjectName("previewTable");
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setMinimumHeight(110);
    m_more = new QLabel;
    m_more->setObjectName("caption");
    m_previewPanel->body()->setContentsMargins(12, 0, 12, 8);
    m_previewPanel->body()->addWidget(m_table, 1);
    m_previewPanel->body()->addWidget(m_more);
    cols.center->addWidget(m_previewPanel, 2);

    // ---------------------------------------------------------------- export settings
    auto* exportPanel = new Panel("Export (Excel/CSV)", "LOCAL");
    m_format = new ModeTabs({"Excel (.xlsx)", "CSV (.csv)"});
    m_seriesCombo = new QComboBox;
    m_fileName = new UnitField(".xlsx");
    m_folder = new QComboBox;
    m_folder->addItem("Documents / PHY104", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104");
    m_folder->addItem("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    m_folder->addItem("Choose folder…", QString());
    auto* include = new QLabel("INCLUDE IN WORKBOOK");
    include->setObjectName("caption");
    auto checkbox = [](const QString& text) { auto* c = new QCheckBox(text); c->setChecked(true); return c; };
    m_chkParams = checkbox("Input parameters and units");
    m_chkSteps = checkbox("Step-by-step results");
    m_chkData = checkbox("Measurement data && fit");
    m_chkNotes = checkbox("Session notes");
    m_ready = new NoticeCard;
    m_primary = new QPushButton;
    m_primary->setObjectName("primary");
    m_secondary = new QPushButton;
    m_secondary->setObjectName("secondary");
    auto* hint = new QLabel("CSV: selected data as UTF-8. Excel also includes notes and calculation steps.");
    hint->setObjectName("hint");
    hint->setWordWrap(true);

    auto* eb = exportPanel->body();
    eb->setSpacing(8);
    eb->addWidget(m_format);
    eb->addWidget(labeled("Data series", m_seriesCombo));
    eb->addWidget(labeled("File name", m_fileName));
    eb->addWidget(labeled("Destination folder", m_folder));
    eb->addWidget(include);
    for (auto* c : {m_chkParams, m_chkSteps, m_chkData, m_chkNotes}) eb->addWidget(c);
    eb->addWidget(m_ready);
    eb->addWidget(m_primary);
    eb->addWidget(m_secondary);
    eb->addWidget(hint);
    eb->addStretch();
    cols.right->addWidget(exportPanel, 1);

    connect(m_format, &ModeTabs::currentChanged, this, &NotesExportPage::onFormatChanged);
    connect(m_seriesCombo, &QComboBox::activated, this, [this] { refreshPreview(); });
    for (auto* c : {m_chkParams, m_chkSteps, m_chkData, m_chkNotes}) connect(c, &QCheckBox::toggled, this, [this] { refreshPreview(); });
    connect(m_folder, &QComboBox::activated, this, [this](int index) {
        if (m_folder->itemData(index).toString().isEmpty()) {
            const QString dir = QFileDialog::getExistingDirectory(this, "Destination folder");
            if (!dir.isEmpty()) { m_folder->insertItem(index, dir, dir); m_folder->setCurrentIndex(index); }
            else m_folder->setCurrentIndex(0);
        }
    });
    connect(m_primary, &QPushButton::clicked, this, &NotesExportPage::exportPrimary);
    connect(m_secondary, &QPushButton::clicked, this, &NotesExportPage::exportSecondary);
    connect(ctx, &AppContext::snapshotChanged, this, &NotesExportPage::refreshSeries);
    connect(ctx, &AppContext::runsChanged, this, &NotesExportPage::refreshPreview);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &NotesExportPage::refreshPreview);
    onFormatChanged();
    refreshSeries();
}

void NotesExportPage::onFormatChanged() {
    const bool excel = m_format->current() == 0;
    m_fileName->setUnit(excel ? ".xlsx" : ".csv");
    m_primary->setText(excel ? "Export workbook" : "Export CSV");
    m_secondary->setText(excel ? "Export as CSV instead" : "Export workbook instead");
    for (auto* c : {m_chkParams, m_chkSteps, m_chkNotes}) c->setEnabled(excel);
    refreshPreview();
}

const DataSeries* NotesExportPage::currentSeries() const {
    const int idx = m_seriesCombo->currentData().toInt();
    if (idx < 0 || idx > 3) return nullptr;
    const Snapshot* snap = m_ctx->log.snapshot(kExperiments[idx]);
    return snap ? &snap->series : nullptr;
}

void NotesExportPage::refreshSeries() {
    const int keep = m_seriesCombo->currentData().isValid() ? m_seriesCombo->currentData().toInt() : 0;
    QSignalBlocker block(m_seriesCombo);
    m_seriesCombo->clear();
    for (int i = 0; i < 4; ++i) {
        const Snapshot* snap = m_ctx->log.snapshot(kExperiments[i]);
        if (!snap || snap->series.rows.empty()) continue;
        m_seriesCombo->addItem(QString("%1 · %2 rows").arg(QString::fromStdString(snap->series.name)).arg(snap->series.rows.size()), i);
    }
    const int at = m_seriesCombo->findData(keep);
    m_seriesCombo->setCurrentIndex(at >= 0 ? at : 0);
    refreshPreview();
}

void NotesExportPage::refreshPreview() {
    const Theme& th = ThemeManager::instance().theme();
    const DataSeries* s = currentSeries();
    m_table->clear();
    if (!s) { m_table->setRowCount(0); m_table->setColumnCount(0); return; }

    m_previewPanel->setTitle("Export preview / " + QString::fromStdString(s->id));
    m_previewPanel->setMeta(QString("%1 ROWS × %2 COLUMNS").arg(s->rows.size()).arg(s->headers.size()));
    const int shown = std::min<int>(7, static_cast<int>(s->rows.size()));
    m_table->setColumnCount(static_cast<int>(s->headers.size()));
    m_table->setRowCount(shown);
    for (int c = 0; c < m_table->columnCount(); ++c) {
        auto* h = new QTableWidgetItem(QString::fromStdString(s->headers[c]));
        h->setForeground(th.amber);
        m_table->setHorizontalHeaderItem(c, h);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int r = 0; r < shown; ++r) {
        for (int c = 0; c < m_table->columnCount(); ++c) {
            const double v = s->rows[r][c];
            auto* item = new QTableWidgetItem(std::isfinite(v) ? QString::number(v, 'f', c == 0 ? 2 : 4) : "n/a");
            item->setForeground(std::isfinite(v) ? th.text : th.dim);
            m_table->setItem(r, c, item);
        }
        m_table->setRowHeight(r, 26);
    }
    const int more = static_cast<int>(s->rows.size()) - shown;
    m_more->setText(more > 0 ? QString("… %1 more row%2 · full precision retained").arg(more).arg(more == 1 ? "" : "s")
                             : "full precision retained");

    const QString base = "PHY104_" + QString::fromStdString(s->id).section('_', 0, 0);
    if (m_fileName->text().isEmpty() || m_fileName->text().startsWith("PHY104_")) m_fileName->setText(base);
    ExportOptions opt{m_chkParams->isChecked(), m_chkSteps->isChecked(), m_chkData->isChecked(), m_chkNotes->isChecked()};
    const QString info = QString::fromStdString(m_format->current() == 0 ? DataExport::describeWorkbook(m_ctx->log, *s, opt)
                                                                          : std::to_string(s->rows.size()) + " rows · " + std::to_string(s->headers.size()) + " columns · UTF-8");
    m_ready->setNotice(m_format->current() == 0 ? "Workbook ready" : "CSV ready", info + ". Numeric cells use full precision.", false);

    const auto runs = m_ctx->log.recent(2);
    QString linked = "Linked runs: ";
    if (runs.empty()) linked += "none yet";
    for (std::size_t i = 0; i < runs.size(); ++i) linked += (i ? " · " : "") + QString::fromStdString(SessionLog::runLabel(runs[i].id));
    m_linked->setText(linked + "   |   Autosaved");
}

QString NotesExportPage::destinationFolder() const { return m_folder->currentData().toString(); }

bool NotesExportPage::exportAs(bool xlsx) {
    const DataSeries* s = currentSeries();
    if (!s) { emit m_ctx->message("Nothing to export yet"); return false; }
    const QString dir = destinationFolder();
    QDir().mkpath(dir);
    QString name = m_fileName->text().trimmed();
    if (name.isEmpty()) name = "PHY104_export";
    const QString path = dir + "/" + name + (xlsx ? ".xlsx" : ".csv");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { emit m_ctx->message("Cannot write " + path); return false; }
    if (xlsx) {
        ExportOptions opt{m_chkParams->isChecked(), m_chkSteps->isChecked(), m_chkData->isChecked(), m_chkNotes->isChecked()};
        const auto bytes = DataExport::buildWorkbook(m_ctx->log, *s, opt);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    } else {
        file.write("\xEF\xBB\xBF");                                   // UTF-8 BOM: Excel shows θ, ₁ correctly
        file.write(QByteArray::fromStdString(DataExport::toCsv(*s)));
    }
    file.close();
    ExportRecord rec;                                                  // the Export page's "recent export" card
    rec.path = path.toStdString();
    rec.format = xlsx ? "Excel (.xlsx)" : "CSV (.csv)";
    rec.series = s->id;
    rec.rows = static_cast<int>(s->rows.size());
    rec.columns = static_cast<int>(s->headers.size());
    rec.when = QDateTime::currentDateTime().toString("dd MMM yyyy · HH:mm").toStdString();
    m_ctx->recordExport(rec);
    return true;
}

void NotesExportPage::exportPrimary() { exportAs(m_format->current() == 0); }
void NotesExportPage::exportSecondary() { exportAs(m_format->current() != 0); }

} // namespace phy104
