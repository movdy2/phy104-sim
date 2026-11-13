#pragma once
#include "BasePage.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QImage>

class QCheckBox;
class QComboBox;
class QPrinter;
class QPushButton;
class QShowEvent;
class QTextBrowser;
class QTimer;

namespace phy104 {

class ExperimentPage;
class ModeTabs;
class NoticeCard;
class Panel;
class RecentExportPanel;
class UnitField;
struct ReportContent;
struct ReportOptions;

/// "PDF Report" - notes, registers, diagrams and charts of every experiment in one printable document.
/// The HTML comes from the tested core builder (`ReportDocument`); this page only collects the inputs,
/// captures the live canvases and hands the document to a QPrinter (PDF file or a real printer).
class ReportPage : public BasePage {
    Q_OBJECT
public:
    explicit ReportPage(AppContext* context, QWidget* parent = nullptr);

    /// The experiment screens whose diagram and chart are captured into the document.
    void setSources(const QList<ExperimentPage*>& pages);

    QTextBrowser* preview() const { return m_preview; }
    QPushButton* saveButton() const { return m_save; }
    int sectionCount() const { return m_sections; }
    QString html() const { return m_html; }

public slots:
    void refreshPreview();
    void savePdf();

protected:
    void showEvent(QShowEvent* event) override;      // the preview is rebuilt when the page opens

private slots:
    void printReport();
    void refreshRecent();

private:
    ReportOptions options(int imageWidth) const;
    ReportContent collect(QMap<QString, QImage>& images) const;
    /// Builds the document (images included) and prints it. Returns false when nothing was written.
    bool renderTo(QPrinter& printer);
    QString destinationFolder() const;
    QString suggestedName() const;

    QList<ExperimentPage*> m_sources;
    QTimer* m_refreshTimer = nullptr;                // the session publishes on every keystroke
    QCheckBox *m_chkNotes = nullptr, *m_chkRegs = nullptr, *m_chkDiagrams = nullptr, *m_chkCharts = nullptr, *m_chkRuns = nullptr;
    ModeTabs* m_paper = nullptr;
    QTextBrowser* m_preview = nullptr;
    Panel *m_optionsPanel = nullptr, *m_previewPanel = nullptr, *m_exportPanel = nullptr;
    UnitField* m_fileName = nullptr;
    QComboBox* m_folder = nullptr;
    QPushButton *m_save = nullptr, *m_print = nullptr;
    NoticeCard* m_ready = nullptr;
    RecentExportPanel* m_recent = nullptr;
    QString m_html;
    int m_sections = 0;
};

} // namespace phy104
