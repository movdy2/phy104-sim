#include "ExportPage.h"
#include "AppContext.h"
#include "Components.h"
#include "DataExport.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

namespace phy104 {
namespace {

QString formatName(bool excel) { return excel ? "Excel (.xlsx)" : "CSV (.csv)"; }

/// "912 B", "12.4 KB", "1.3 MB" - used by the recent-export card.
QString fileSize(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

QString seriesBaseName(const std::string& id) { return "PHY104_" + QString::fromStdString(id).section('_', 0, 0); }

} // namespace

ExportPage::ExportPage(AppContext* ctx, QWidget* parent)
    : BasePage("Export (Excel/CSV)", "REPORT DATA / What the series contains, every row of the table, and the file you last wrote",
               ctx, parent) {
    auto cols = addThreeColumns(252, 268);

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
    auto* hint = new QLabel("CSV: the chosen series as UTF-8. Excel also includes notes and calculation steps.");
    hint->setObjectName("hint");
    hint->setWordWrap(true);

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
    sb->addWidget(hint);
    sb->addStretch();
    cols.right->addWidget(settings, 1);

    auto* recent = new Panel("Recent export", "REMEMBERED");
    m_recentFile = new QLabel("No export yet");
    m_recentFile->setObjectName("sectionTitle");
    m_recentFile->setWordWrap(true);
    m_recentMeta = new QLabel("—");
    m_recentMeta->setObjectName("caption");
    m_recentMeta->setWordWrap(true);
    m_recentWhen = new QLabel("The file you export last is remembered here and in the .phy session.");
    m_recentWhen->setObjectName("hint");
    m_recentWhen->setWordWrap(true);
    m_openFolder = new QPushButton("Open folder");
    m_openFolder->setObjectName("secondary");
    recent->body()->setSpacing(6);
    recent->body()->addWidget(m_recentFile);
    recent->body()->addWidget(m_recentMeta);
    recent->body()->addWidget(m_recentWhen);
    recent->body()->addWidget(m_openFolder);
    cols.right->addWidget(recent, 0);

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
    connect(m_openFolder, &QPushButton::clicked, this, &ExportPage::openDestination);
    connect(ctx, &AppContext::snapshotChanged, this, &ExportPage::refreshSeries);
    connect(ctx, &AppContext::exportRecorded, this, &ExportPage::refreshRecent);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] { refreshPreview(); refreshRecent(); });

    StatusInfo st;
    st.left = "Ready to export";
    st.mid = "Excel + CSV";
    st.right = "Local files only";
    setStatusInfo(st, false);

    onFormatChanged();
    refreshSeries();
    refreshRecent();
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
        m_openFolder->setEnabled(m_ctx->log.hasExport());
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
    const QString base = seriesBaseName(s.id);
    if (m_fileName->text().isEmpty() || m_fileName->text().startsWith("PHY104_")) m_fileName->setText(base);
    const ExportOptions opt{m_chkParams->isChecked(), m_chkSteps->isChecked(), m_chkData->isChecked(), m_chkNotes->isChecked()};
    const QString info = excel ? QString::fromStdString(DataExport::describeWorkbook(m_ctx->log, s, opt))
                               : QString("%1 rows · %2 columns · UTF-8").arg(rows).arg(cols);
    m_ready->setNotice(excel ? "Workbook ready" : "CSV ready", info + ". Numeric cells use full precision.", false);
    m_primary->setEnabled(true);
    m_secondary->setEnabled(true);
    m_openFolder->setEnabled(m_ctx->log.hasExport());
}

void ExportPage::refreshRecent() {
    const ExportRecord& rec = m_ctx->log.lastExport();
    if (rec.path.empty()) {
        m_recentFile->setText("No export yet");
        m_recentMeta->setText("—");
        m_recentWhen->setText("The file you export last is remembered here and in the .phy session.");
        m_openFolder->setEnabled(false);
        return;
    }
    const QFileInfo info(QString::fromStdString(rec.path));
    m_recentFile->setText(info.fileName());
    QString meta = QString::fromStdString(rec.format) + QString(" · %1 rows × %2 columns").arg(rec.rows).arg(rec.columns);
    if (info.exists()) meta += " · " + fileSize(info.size());
    m_recentMeta->setText(meta);
    m_recentWhen->setText(QString::fromStdString(rec.when) + "\n" + info.absolutePath() +
                          (info.exists() ? QString() : QString("  (file no longer there)")));
    m_openFolder->setEnabled(true);
}

void ExportPage::setFormat(bool xlsx) {
    m_format->setCurrent(xlsx ? 0 : 1);      // setCurrent does not emit currentChanged
    onFormatChanged();
}

void ExportPage::onFormatChanged() {
    const bool excel = m_format->current() == 0;
    m_fileName->setUnit(excel ? ".xlsx" : ".csv");
    m_primary->setText(excel ? "Export workbook" : "Export CSV");
    m_secondary->setText(excel ? "Export as CSV instead" : "Export workbook instead");
    for (auto* c : {m_chkParams, m_chkSteps, m_chkNotes}) c->setEnabled(excel);   // CSV is the series only
    refreshPreview();
}

bool ExportPage::exportAs(bool xlsx) {
    const Snapshot* snap = currentSnapshot();
    if (!snap || snap->series.rows.empty()) { emit m_ctx->message("Nothing to export yet"); return false; }
    const DataSeries& s = snap->series;
    const QString dir = destinationFolder();
    if (dir.isEmpty()) { emit m_ctx->message("Choose a destination folder first"); return false; }

    QString name = m_fileName->text().trimmed();
    if (name.isEmpty()) name = seriesBaseName(s.id);
    if (name.endsWith(".xlsx", Qt::CaseInsensitive)) name.chop(5);
    else if (name.endsWith(".csv", Qt::CaseInsensitive)) name.chop(4);
    if (name.isEmpty()) name = seriesBaseName(s.id);

    QDir().mkpath(dir);
    const QString path = dir + "/" + name + (xlsx ? ".xlsx" : ".csv");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { emit m_ctx->message("Cannot write " + path); return false; }
    if (xlsx) {
        const ExportOptions opt{m_chkParams->isChecked(), m_chkSteps->isChecked(), m_chkData->isChecked(), m_chkNotes->isChecked()};
        const auto bytes = DataExport::buildWorkbook(m_ctx->log, s, opt);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    } else {
        file.write("\xEF\xBB\xBF");                                   // UTF-8 BOM: Excel shows θ, ₁ correctly
        file.write(QByteArray::fromStdString(DataExport::toCsv(s)));
    }
    file.close();

    ExportRecord rec;
    rec.path = path.toStdString();
    rec.format = formatName(xlsx).toStdString();
    rec.series = s.id;
    rec.rows = static_cast<int>(s.rows.size());
    rec.columns = static_cast<int>(s.headers.size());
    rec.when = QDateTime::currentDateTime().toString("dd MMM yyyy · HH:mm").toStdString();
    m_ctx->recordExport(rec);
    refreshRecent();
    return true;
}

void ExportPage::exportPrimary() { exportAs(m_format->current() == 0); }
void ExportPage::exportSecondary() { exportAs(m_format->current() != 0); }

void ExportPage::openDestination() {
    const QString path = QString::fromStdString(m_ctx->log.lastExport().path);
    if (path.isEmpty()) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

} // namespace phy104
