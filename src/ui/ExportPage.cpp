#include "ExportPage.h"
#include "AppContext.h"
#include "Components.h"
#include "DataExport.h"
#include "ExportService.h"
#include "RecentExportPanel.h"
#include "ReportPage.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {

ExportPage::ExportPage(AppContext* ctx, QWidget* parent)
    : BasePage("Export", "CSV, Excel and PDF reports / Prepare complete data files or a formatted laboratory report",
               ctx, parent) {
    m_exportModes = new ModeTabs({"Series data", "PDF report"});
    m_exportModes->setObjectName("exportModeTabs");
    content()->addWidget(m_exportModes);
    m_workspace = new QStackedWidget;
    m_workspace->setObjectName("exportWorkspace");
    content()->addWidget(m_workspace, 1);

    auto* dataPane = new QWidget;
    auto* columnsRow = new QHBoxLayout(dataPane);
    columnsRow->setContentsMargins(0, 0, 0, 0);
    columnsRow->setSpacing(12);
    Columns cols;
    auto makeColumn = [columnsRow](int width) {
        auto* column = new QVBoxLayout;
        column->setSpacing(12);
        auto* holder = new QWidget;
        holder->setLayout(column);
        column->setContentsMargins(0, 0, 0, 0);
        if (width > 0) holder->setFixedWidth(width);
        columnsRow->addWidget(holder, width > 0 ? 0 : 1);
        return column;
    };
    cols.left = makeColumn(252);
    cols.center = makeColumn(0);
    cols.right = makeColumn(268);
    m_workspace->addWidget(dataPane);

    // ---------------------------------------------------------------- left: the series and its description
    m_listPanel = new Panel("Data series", "none");
    auto* listPanel = m_listPanel;
    m_series = new QListWidget;
    m_series->setObjectName("seriesList");
    m_series->setMinimumHeight(150);
    listPanel->body()->addWidget(m_series, 1);
    cols.left->addWidget(listPanel, 3);

    auto* descPanel = new Panel("Series description", "SOURCE");
    m_descTitle = new QLabel("No data series yet");
    m_descTitle->setObjectName("sectionTitle");
    m_descTitle->setWordWrap(true);
    m_descBody = new QLabel;
    m_descBody->setObjectName("hint");
    m_descBody->setWordWrap(true);
    m_descNote = new QLabel;
    m_descNote->setObjectName("hint");
    m_descNote->setWordWrap(true);
    m_descColumns = new QLabel;
    m_descColumns->setObjectName("caption");
    m_descColumns->setWordWrap(true);
    descPanel->body()->setSpacing(6);
    descPanel->body()->addWidget(m_descTitle);
    descPanel->body()->addWidget(m_descBody);
    descPanel->body()->addWidget(m_descNote);
    descPanel->body()->addSpacing(2);
    descPanel->body()->addWidget(m_descColumns);
    descPanel->body()->addStretch();
    cols.left->addWidget(descPanel, 2);

    // ---------------------------------------------------------------- centre: every row, not a sample
    m_previewPanel = new Panel("Full preview", "");
    m_table = new QTableWidget;
    m_table->setObjectName("previewTable");
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_previewPanel->body()->setContentsMargins(12, 0, 12, 8);
    m_previewPanel->body()->addWidget(m_table, 1);
    m_previewFoot = new QLabel;
    m_previewFoot->setObjectName("caption");
    m_previewFoot->setWordWrap(true);
    m_previewPanel->body()->addWidget(m_previewFoot);
    cols.center->addWidget(m_previewPanel, 1);

    // ---------------------------------------------------------------- right: settings + recent export
    auto* settings = new Panel("Export settings", "LOCAL");
    m_format = new ModeTabs({"Excel (.xlsx)", "CSV (.csv)"});
    m_format->setObjectName("seriesFormatTabs");
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
    m_print = new QPushButton("Print…");
    m_print->setObjectName("secondary");
    auto* sb = settings->body();
    sb->setSpacing(8);
    sb->addWidget(m_format);
    sb->addWidget(labeled("File name", m_fileName));
    sb->addWidget(labeled("Destination folder", m_folder));
    sb->addWidget(include);
    for (auto* c : {m_chkParams, m_chkSteps, m_chkData, m_chkNotes}) sb->addWidget(c);
    sb->addWidget(m_ready);
    sb->addWidget(m_primary);
    sb->addWidget(m_secondary);
    sb->addWidget(m_print);
    sb->addStretch();
    cols.right->addWidget(settings, 1);

    m_recent = new RecentExportPanel;
    cols.right->addWidget(m_recent, 0);

    connect(m_exportModes, &ModeTabs::currentChanged, this, &ExportPage::onExportPaneChanged);
    connect(m_format, &ModeTabs::currentChanged, this, &ExportPage::onFormatChanged);
    connect(m_series, &QListWidget::currentRowChanged, this, [this](int) { refreshPreview(); });
    connect(m_series, &QListWidget::itemDoubleClicked, this, [this] { m_primary->click(); });
    for (auto* c : {m_chkParams, m_chkSteps, m_chkData, m_chkNotes}) connect(c, &QCheckBox::toggled, this, [this] { refreshPreview(); });
    connect(m_folder, &QComboBox::activated, this, [this](int index) {
        if (m_folder->itemData(index).toString().isEmpty()) {
            const QString dir = QFileDialog::getExistingDirectory(this, "Destination folder");
            if (!dir.isEmpty()) { m_folder->insertItem(index, dir, dir); m_folder->setCurrentIndex(index); }
            else m_folder->setCurrentIndex(0);
        }
    });
    connect(m_primary, &QPushButton::clicked, this, &ExportPage::exportPrimary);
    connect(m_secondary, &QPushButton::clicked, this, &ExportPage::exportSecondary);
    connect(m_print, &QPushButton::clicked, this, &ExportPage::printTable);
    connect(ctx, &AppContext::snapshotChanged, this, &ExportPage::refreshSeries);
    connect(ctx, &AppContext::exportRecorded, this, &ExportPage::refreshRecent);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { refreshPreview(); refreshRecent(); });

    StatusInfo st;
    st.left = "Ready to export";
    st.mid = "Excel + CSV";
    st.right = "Local files only";
    setStatusInfo(st, false);

    onFormatChanged();
    onExportPaneChanged();
    refreshSeries();
    refreshRecent();
}

void ExportPage::setReportPage(ReportPage* reportPage) {
    if (!reportPage || m_reportPage) return;
    m_reportPage = reportPage;
    m_workspace->addWidget(reportPage);
    connect(reportPage, &BasePage::stateChanged, this, [this] {
        if (m_exportModes->current() == 1 && m_reportPage)
            setStatusInfo(m_reportPage->statusInfo(), m_reportPage->isPaused());
    });
    onExportPaneChanged();
}

void ExportPage::showDataExport() {
    m_exportModes->setCurrent(0);
    onExportPaneChanged();
}

void ExportPage::showPdfReport() {
    m_exportModes->setCurrent(1);
    onExportPaneChanged();
}

void ExportPage::onExportPaneChanged() {
    int pane = m_exportModes->current();
    if (pane == 1 && !m_reportPage) {
        m_exportModes->setCurrent(0);
        pane = 0;
    }
    m_workspace->setCurrentIndex(pane);
    if (pane == 1) {
        setStatusInfo(m_reportPage->statusInfo(), m_reportPage->isPaused());
        return;
    }
    StatusInfo st;
    st.left = "Ready to export";
    st.mid = m_format->current() == 0 ? "Excel workbook" : "CSV file";
    st.right = "Local files only";
    setStatusInfo(st, false);
}

QString ExportPage::destinationFolder() const { return m_folder->currentData().toString(); }

const Snapshot* ExportPage::currentSnapshot() const {
    const QListWidgetItem* item = m_series->currentItem();
    if (!item) return nullptr;
    return m_ctx->log.snapshot(item->data(Qt::UserRole).toString().toStdString());
}

void ExportPage::refreshSeries() {
    const QString keep = m_series->currentItem() ? m_series->currentItem()->data(Qt::UserRole).toString() : QString();
    {
        const QSignalBlocker block(m_series);
        m_series->clear();
        for (const auto& [name, snap] : m_ctx->log.snapshots()) {
            if (snap.series.rows.empty()) continue;
            auto* item = new QListWidgetItem(QString::fromStdString(snap.series.name) + "\n" +
                                             QString("%1 rows · %2 columns").arg(snap.series.rows.size()).arg(snap.series.headers.size()));
            item->setData(Qt::UserRole, QString::fromStdString(name));
            m_series->addItem(item);
        }
        int row = 0;
        for (int i = 0; i < m_series->count(); ++i)
            if (m_series->item(i)->data(Qt::UserRole).toString() == keep) { row = i; break; }
        if (m_series->count() > 0) m_series->setCurrentRow(row);
    }
    m_listPanel->setMeta(QString("%1 SERIES").arg(m_series->count()));
    refreshPreview();
}

void ExportPage::refreshPreview() {
    const Snapshot* snap = currentSnapshot();
    m_table->clear();
    if (!snap || snap->series.rows.empty()) {
        m_previewPanel->setTitle("Full preview");
        m_previewPanel->setMeta("no data");
        m_table->setRowCount(0);
        m_table->setColumnCount(0);
        m_previewFoot->setText("Run a calculation on an experiment page: its table is published here automatically.");
        m_descTitle->setText("No data series yet");
        m_descBody->setText("Every experiment page publishes its data series while you type, so this list fills up as you work.");
        m_descNote->setText(QString());
        m_descColumns->setText(QString());
        m_ready->setNotice("Nothing to export", "Choose or create a series that has rows.", true);
        m_primary->setEnabled(false);
        m_secondary->setEnabled(false);
        m_print->setEnabled(false);
        return;
    }

    const DataSeries& s = snap->series;
    const bool excel = m_format->current() == 0;
    const Theme& th = ThemeManager::instance().theme();

    // ---- what the series contains
    m_descTitle->setText(QString::fromStdString(s.name));
    m_descBody->setText(QString::fromStdString(s.description));
    m_descNote->setText(QString::fromStdString(s.note));
    QStringList columns;
    for (const auto& h : s.headers) columns << QString::fromStdString(h);
    m_descColumns->setText("COLUMNS\n" + columns.join(" · "));

    // ---- every row, full width
    m_previewPanel->setTitle("Full preview / " + QString::fromStdString(s.id));
    m_previewPanel->setMeta(QString("%1 ROWS × %2 COLUMNS").arg(s.rows.size()).arg(s.headers.size()));
    const int rows = static_cast<int>(s.rows.size());
    const int cols = static_cast<int>(s.headers.size());
    m_table->setColumnCount(cols);
    m_table->setRowCount(rows);
    for (int c = 0; c < cols; ++c) {
        auto* h = new QTableWidgetItem(QString::fromStdString(s.headers[c]));
        h->setForeground(th.amber);
        m_table->setHorizontalHeaderItem(c, h);
    }
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const double v = s.rows[r][c];
            auto* item = new QTableWidgetItem(std::isfinite(v) ? QString::number(v, 'f', c == 0 ? 2 : 4) : "n/a");
            item->setForeground(std::isfinite(v) ? th.text : th.dim);
            m_table->setItem(r, c, item);
        }
        m_table->setRowHeight(r, 26);
    }
    m_previewFoot->setText(QString("%1 rows × %2 columns · every row shown · full precision retained").arg(rows).arg(cols));

    // ---- file name + what the export will contain
    const QString base = QString::fromStdString(DataExport::suggestedFileName(s, ""));
    if (m_fileName->text().isEmpty() || m_fileName->text().startsWith("PHY104_")) m_fileName->setText(base);
    const ExportOptions opt{m_chkParams->isChecked(), m_chkSteps->isChecked(), m_chkData->isChecked(), m_chkNotes->isChecked()};
    const QString info = excel ? QString::fromStdString(DataExport::describeWorkbook(m_ctx->log, s, opt))
                               : QString("%1 rows · %2 columns · UTF-8").arg(rows).arg(cols);
    m_ready->setNotice(excel ? "Workbook ready" : "CSV ready", info + ". Numeric cells use full precision.", false);
    m_primary->setEnabled(true);
    m_print->setEnabled(true);
    m_secondary->setEnabled(true);
}

void ExportPage::refreshRecent() {
    m_recent->setRecord(m_ctx->log.lastExport());
}

void ExportPage::setFormat(bool xlsx) {
    m_format->setCurrent(xlsx ? 0 : 1);      // setCurrent does not emit currentChanged
    onFormatChanged();
    showDataExport();
}

void ExportPage::onFormatChanged() {
    const bool excel = m_format->current() == 0;
    m_fileName->setUnit(excel ? ".xlsx" : ".csv");
    m_primary->setText(excel ? "Export workbook" : "Export CSV");
    m_secondary->setText(excel ? "Export as CSV instead" : "Export workbook instead");
    for (auto* c : {m_chkParams, m_chkSteps, m_chkNotes}) c->setEnabled(excel);   // CSV is the series only
    refreshPreview();
    if (m_exportModes->current() == 0) onExportPaneChanged();
}

bool ExportPage::exportAs(bool xlsx) {
    const Snapshot* snap = currentSnapshot();
    if (!snap || snap->series.rows.empty()) { emit m_ctx->message("Nothing to export yet"); return false; }
    const DataSeries& s = snap->series;
    const QString dir = destinationFolder();
    if (dir.isEmpty()) { emit m_ctx->message("Choose a destination folder first"); return false; }

    const QString extension = xlsx ? ".xlsx" : ".csv";
    const QString fallback = QString::fromStdString(DataExport::suggestedFileName(s, extension.toStdString()));
    const QString path = ExportService::outputPath(dir, m_fileName->text(), fallback, extension);
    const ExportOptions options{m_chkParams->isChecked(), m_chkSteps->isChecked(),
                                m_chkData->isChecked(), m_chkNotes->isChecked()};
    const auto format = xlsx ? SeriesExportFormat::Workbook : SeriesExportFormat::Csv;
    QString error;
    if (!ExportService::writeSeriesFile(path, m_ctx->log, s, format, options, &error)) {
        emit m_ctx->message(error);
        return false;
    }

    const ExportRecord rec = ExportService::makeRecord(
        path, xlsx ? "Excel (.xlsx)" : "CSV (.csv)", s.id,
        static_cast<int>(s.rows.size()), static_cast<int>(s.headers.size()));
    m_ctx->recordExport(rec);
    refreshRecent();
    return true;
}

void ExportPage::printTable() {
    const Snapshot* snap = currentSnapshot();
    if (!snap || snap->series.rows.empty()) { emit m_ctx->message("Nothing to print: publish a sweep first"); return; }
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName("PHY104 data");
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle("Print the data table");
    if (dialog.exec() != QDialog::Accepted) return;

    // the same table as the preview, as simple HTML (no CSS tricks: Qt's rich text engine prints it as it is shown)
    QString html = "<h2>" + QString::fromStdString(snap->series.name).toHtmlEscaped() + "</h2><p>"
                 + QString::fromStdString(snap->series.description).toHtmlEscaped() + "</p><table border='1' cellspacing='0' cellpadding='4'><tr>";
    for (const std::string& h : snap->series.headers) html += "<th>" + QString::fromStdString(h).toHtmlEscaped() + "</th>";
    html += "</tr>";
    for (const auto& row : snap->series.rows) {
        html += "<tr>";
        for (double v : row) html += "<td>" + (std::isnan(v) ? QString("—") : QString::number(v, 'g', 6)) + "</td>";
        html += "</tr>";
    }
    html += "</table>";
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(&printer);
    emit m_ctx->message("Printed " + QString::fromStdString(snap->series.name));
}

void ExportPage::exportPrimary() { exportAs(m_format->current() == 0); }
void ExportPage::exportSecondary() { exportAs(m_format->current() != 0); }

} // namespace phy104
