#pragma once
#include "BasePage.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace phy104 {

struct Snapshot;
class ModeTabs;
class NoticeCard;
class Panel;
class RecentExportPanel;
class ReportPage;
class UnitField;

/// The single Export destination: a full-series CSV/Excel workspace and an academic PDF-report workspace
/// share one sidebar entry, one page header and the same recent-export history.
class ExportPage : public BasePage {
    Q_OBJECT
public:
    explicit ExportPage(AppContext* context, QWidget* parent = nullptr);

    QListWidget* seriesList() const { return m_series; }
    QTableWidget* previewTable() const { return m_table; }
    /// Selects the series-export pane and preselects Excel or CSV before the page is shown.
    void setFormat(bool xlsx);
    void showDataExport();
    void showPdfReport();
    void setReportPage(ReportPage* reportPage);

private slots:
    void refreshSeries();
    void refreshPreview();
    void refreshRecent();
    void onFormatChanged();
    void onExportPaneChanged();
    void printTable();                       // Print… : the table of the selected series, on paper or as PDF
    void exportPrimary();
    void exportSecondary();

private:
    const Snapshot* currentSnapshot() const;
    bool exportAs(bool xlsx);
    QString destinationFolder() const;

    ModeTabs* m_exportModes = nullptr;
    QStackedWidget* m_workspace = nullptr;
    ReportPage* m_reportPage = nullptr;
    Panel* m_listPanel;
    QListWidget* m_series;
    QLabel *m_descTitle, *m_descBody, *m_descNote, *m_descColumns;
    Panel* m_previewPanel;
    QTableWidget* m_table;
    QLabel* m_previewFoot;
    ModeTabs* m_format;
    UnitField* m_fileName;
    QComboBox* m_folder;
    QCheckBox *m_chkParams, *m_chkSteps, *m_chkData, *m_chkNotes;
    NoticeCard* m_ready;
    QPushButton *m_primary, *m_secondary, *m_print;
    RecentExportPanel* m_recent = nullptr;
};

} // namespace phy104
