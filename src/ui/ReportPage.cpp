#include "ReportPage.h"
#include "AppContext.h"
#include "Components.h"
#include "ExperimentPages.h"
#include "ReportDocument.h"
#include "ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QLabel>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QToolButton>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace phy104 {
namespace {

/// Renders a widget (even a hidden one inside the page stack) into a 2x image for print quality.
QImage grabWidget(QWidget* widget) {
    if (!widget || widget->width() < 60 || widget->height() < 60) return {};
    constexpr qreal scale = 2.0;
    QImage image(widget->size() * scale, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.scale(scale, scale);
    widget->render(&painter);
    painter.end();
    return image;
}

} // namespace

ReportPage::ReportPage(AppContext* ctx, QWidget* parent)
    : BasePage("PDF Report", "PRINT / Notes, registers, diagrams and charts of every experiment in one document",
               ctx, parent) {
    exportButton()->hide();            // this IS the PDF screen: no menu pointing at itself
    auto cols = addThreeColumns(252, 268);

    // ---------------------------------------------------------------- left: what goes into the document
    m_optionsPanel = new Panel("Report contents", "LIVE");
    auto* options = m_optionsPanel->body();
    options->setSpacing(8);
    auto checkbox = [this](const QString& text, bool on = true) {
        auto* c = new QCheckBox(text);
        c->setChecked(on);
        connect(c, &QCheckBox::toggled, this, [this] { refreshPreview(); });
        return c;
    };
    m_chkDiagrams = checkbox("Diagrams of every experiment");
    m_chkCharts = checkbox("Charts (intensity, fringes, sweeps)");
    m_chkRegs = checkbox("Parameters and LCD registers");
    m_chkNotes = checkbox("Session notes");
    m_chkRuns = checkbox("Saved calculations");
    m_paper = new ModeTabs({"A4 portrait", "A4 landscape"});
    connect(m_paper, &ModeTabs::currentChanged, this, [this] { refreshPreview(); });

    auto* include = new QLabel("INCLUDE");
    include->setObjectName("caption");
    auto* pageLabel = new QLabel("PAPER");
    pageLabel->setObjectName("caption");
    options->addWidget(include);
    for (QCheckBox* c : {m_chkDiagrams, m_chkCharts, m_chkRegs, m_chkNotes, m_chkRuns}) options->addWidget(c);
    options->addSpacing(4);
    options->addWidget(pageLabel);
    options->addWidget(m_paper);
    auto* hint = new QLabel("The whole document is built from the session: nothing leaves this computer.");
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    options->addWidget(hint);
    options->addStretch();
    cols.left->addWidget(m_optionsPanel, 1);

    // ---------------------------------------------------------------- centre: the page as it will print
    m_previewPanel = new Panel("Preview", "PAGE 1");
    m_preview = new QTextBrowser;
    m_preview->setObjectName("reportPreview");
    m_preview->setOpenExternalLinks(false);
    m_preview->setStyleSheet("QTextBrowser#reportPreview { background: #ffffff; border: 1px solid #d5c4a1; }");
    m_previewPanel->body()->setContentsMargins(12, 0, 12, 8);
    m_previewPanel->body()->addWidget(m_preview, 1);
    cols.center->addWidget(m_previewPanel, 1);

    // ---------------------------------------------------------------- right: where it goes
    m_exportPanel = new Panel("Export", "PDF");
    auto* body = m_exportPanel->body();
    body->setSpacing(8);
    m_fileName = new UnitField(".pdf");
    m_fileName->setText(suggestedName());
    m_folder = new QComboBox;
    m_folder->addItem("Documents / PHY104", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/PHY104");
    m_folder->addItem("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    m_folder->addItem("Choose folder…", QString());
    connect(m_folder, &QComboBox::activated, this, [this](int index) {
        if (m_folder->itemData(index).toString().isEmpty()) {
            const QString dir = QFileDialog::getExistingDirectory(this, "Destination folder");
            if (!dir.isEmpty()) {
                m_folder->insertItem(index, dir, dir);
                m_folder->setCurrentIndex(index);
            } else {
                m_folder->setCurrentIndex(0);
            }
        }
    });
    m_ready = new NoticeCard;
    m_save = new QPushButton("Save PDF");
    m_save->setObjectName("primary");
    m_print = new QPushButton("Print…");
    m_print->setObjectName("secondary");
    auto* footer = new QLabel("Save PDF writes the file directly into the folder above; Print… opens the system dialog.");
    footer->setObjectName("hint");
    footer->setWordWrap(true);
    body->addWidget(labeled("File name", m_fileName));
    body->addWidget(labeled("Destination folder", m_folder));
    body->addWidget(m_ready);
    body->addWidget(m_save);
    body->addWidget(m_print);
    body->addWidget(footer);
    body->addStretch();
    cols.right->addWidget(m_exportPanel, 1);

    auto* recent = new Panel("Recent export", "REMEMBERED");
    m_recentFile = new QLabel("No export yet");
    m_recentFile->setObjectName("sectionTitle");
    m_recentFile->setWordWrap(true);
    m_recentMeta = new QLabel("The report you write last is remembered here and in the .phy session.");
    m_recentMeta->setObjectName("hint");
    m_recentMeta->setWordWrap(true);
    recent->body()->setSpacing(6);
    recent->body()->addWidget(m_recentFile);
    recent->body()->addWidget(m_recentMeta);
    cols.right->addWidget(recent, 0);

    connect(m_save, &QPushButton::clicked, this, &ReportPage::savePdf);
    connect(m_print, &QPushButton::clicked, this, &ReportPage::printReport);
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(250);                                      // the session publishes on every keystroke
    connect(m_refreshTimer, &QTimer::timeout, this, &ReportPage::refreshPreview);
    connect(ctx, &AppContext::snapshotChanged, this, [this] { if (isVisible()) m_refreshTimer->start(); });
    connect(ctx, &AppContext::runsChanged, this, [this] { if (isVisible()) m_refreshTimer->start(); });
    connect(ctx, &AppContext::notesSaved, this, [this] { if (isVisible()) m_refreshTimer->start(); });
    connect(ctx, &AppContext::exportRecorded, this, &ReportPage::refreshRecent);

    StatusInfo st;
    st.left = "Report ready";
    st.mid = "A4 · PDF";
    st.right = "Local files only";
    setStatusInfo(st, false);

    refreshPreview();
    refreshRecent();
}

void ReportPage::showEvent(QShowEvent* event) {
    BasePage::showEvent(event);
    refreshPreview();                                                      // the screens changed while we were away
}

void ReportPage::setSources(const QList<ExperimentPage*>& pages) {
    m_sources = pages;
    refreshPreview();
}

QString ReportPage::suggestedName() const {
    return QString::fromStdString(reportFileStem(QDate::currentDate().toString("yyyy-MM-dd").toStdString()));
}

QString ReportPage::destinationFolder() const { return m_folder->currentData().toString(); }

ReportOptions ReportPage::options(int imageWidth) const {
    ReportOptions o;
    o.generated = QDateTime::currentDateTime().toString("d MMMM yyyy · HH:mm").toStdString();
    o.sessionName = m_ctx->sessionFile.toStdString();
    o.notes = m_chkNotes->isChecked();
    o.registers = m_chkRegs->isChecked();
    o.diagrams = m_chkDiagrams->isChecked();
    o.charts = m_chkCharts->isChecked();
    o.runs = m_chkRuns->isChecked();
    o.imageWidth = imageWidth;
    return o;
}

ReportContent ReportPage::collect(QMap<QString, QImage>& images) const {
    ReportContent content;
    content.notes = m_ctx->log.notes();
    content.runs = m_ctx->log.runs();
    for (ExperimentPage* page : m_sources) {
        if (!page) continue;
        const QString name = page->experimentName();
        const Snapshot* snap = m_ctx->log.snapshot(name.toStdString());
        if (!snap) continue;
        ReportSection section;
        section.title = name.toStdString();
        section.headline = snap->headline;
        section.parameters = snap->parameters;
        section.registers = snap->registers;
        if (m_chkDiagrams->isChecked()) {
            const QString key = QString::fromStdString(reportImageKey("diagram", section.title));
            QImage image = grabWidget(page->diagramCanvas());
            if (!image.isNull()) {
                section.diagramKey = key.toStdString();
                images.insert(key, image);
            }
        }
        if (m_chkCharts->isChecked()) {
            const QString key = QString::fromStdString(reportImageKey("chart", section.title));
            QImage image = grabWidget(page->chartWidget());
            if (!image.isNull()) {
                section.chartKey = key.toStdString();
                images.insert(key, image);
            }
        }
        content.sections.push_back(section);
    }
    return content;
}

void ReportPage::refreshPreview() {
    QMap<QString, QImage> images;
    const ReportContent content = collect(images);
    m_html = QString::fromStdString(reportHtml(content, options(380)));       // preview column width
    m_sections = static_cast<int>(content.sections.size());

    QTextDocument* doc = m_preview->document();
    doc->clear();
    for (auto it = images.cbegin(); it != images.cend(); ++it)
        doc->addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
    QTextCursor cursor(doc);
    cursor.insertHtml(m_html);

    m_previewPanel->setMeta(m_sections == 1 ? "1 EXPERIMENT" : QString("%1 EXPERIMENTS").arg(m_sections));
    const bool anything = m_sections > 0 || (m_chkNotes->isChecked() && !content.notes.empty()) ||
                          (m_chkRuns->isChecked() && !content.runs.empty());
    m_save->setEnabled(anything);
    m_print->setEnabled(anything);
    if (!anything)
        m_ready->setNotice("Nothing to print yet", "Run a calculation, write a note or tick another section.", true);
    else
        m_ready->setNotice(QString("%1 experiment(s) · %2 run(s)").arg(m_sections).arg(static_cast<int>(content.runs.size())),
                           "Diagrams and charts are captured from the live screens, so what you see is what prints.", false);
}

bool ReportPage::renderTo(QPrinter& printer) {
    QMap<QString, QImage> images;
    const ReportContent content = collect(images);
    const ReportOptions o = options(520);                                    // A4 text width
    QTextDocument doc;
    doc.setDefaultFont(QFont("Inter", 10));
    for (auto it = images.cbegin(); it != images.cend(); ++it)
        doc.addResource(QTextDocument::ImageResource, QUrl(it.key()), it.value());
    QTextCursor cursor(&doc);
    cursor.insertHtml(QString::fromStdString(reportHtml(content, o)));
    doc.print(&printer);
    return true;
}

void ReportPage::savePdf() {
    const QString dir = destinationFolder();
    if (dir.isEmpty()) {
        emit m_ctx->message("Choose a destination folder first");
        return;
    }
    QString name = m_fileName->text().trimmed();
    if (name.isEmpty()) name = suggestedName();
    if (name.endsWith(".pdf", Qt::CaseInsensitive)) name.chop(4);
    if (name.isEmpty()) name = suggestedName();
    QDir().mkpath(dir);
    const QString path = dir + "/" + name + ".pdf";

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(m_paper->current() == 1 ? QPageLayout::Landscape : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    if (!renderTo(printer)) {
        emit m_ctx->message("Could not write " + path);
        return;
    }

    ExportRecord rec;
    rec.path = path.toStdString();
    rec.format = "PDF report";
    rec.series = "report";
    rec.when = QDateTime::currentDateTime().toString("d MMM yyyy · HH:mm").toStdString();
    rec.rows = static_cast<int>(m_ctx->log.runs().size());
    rec.columns = m_sections;
    m_ctx->recordExport(rec);
    emit m_ctx->message("PDF report written: " + QFileInfo(path).fileName());
    refreshRecent();
}

void ReportPage::printReport() {
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(m_paper->current() == 1 ? QPageLayout::Landscape : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle("Print laboratory report");
    if (dialog.exec() != QDialog::Accepted) return;
    renderTo(printer);
}

void ReportPage::refreshRecent() {
    if (!m_ctx->log.hasExport()) {
        m_recentFile->setText("No export yet");
        m_recentMeta->setText("The report you write last is remembered here and in the .phy session.");
        return;
    }
    const ExportRecord& rec = m_ctx->log.lastExport();
    m_recentFile->setText(QFileInfo(QString::fromStdString(rec.path)).fileName());
    m_recentMeta->setText(QString::fromStdString(rec.format) + " · " + QString::fromStdString(rec.when) + " · " +
                          QFileInfo(QString::fromStdString(rec.path)).absolutePath());
}

} // namespace phy104
