#pragma once
#include "BasePage.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace phy104 {

struct Snapshot;
class ModeTabs;
class NoticeCard;
class Panel;
class UnitField;

/// "Export (Excel/CSV)" - the dedicated export frame of the second Figma batch:
/// the data series and what it contains (left), the full table (centre), the export settings and the
/// "recent export" card that remembers the last file in the .phy session (right).
class ExportPage : public BasePage {
    Q_OBJECT
public:
    explicit ExportPage(AppContext* context, QWidget* parent = nullptr);

    QListWidget* seriesList() const { return m_series; }
    QTableWidget* previewTable() const { return m_table; }
    /// The header export icon picks the format before the page is shown (1 = Excel, 2 = CSV).
    void setFormat(bool xlsx);

private slots:
    void refreshSeries();
    void refreshPreview();
    void refreshRecent();
    void onFormatChanged();
    void printTable();                       // Print… : the table of the selected series, on paper or as PDF
    void exportPrimary();
    void exportSecondary();
    void openDestination();

private:
    const Snapshot* currentSnapshot() const;
    bool exportAs(bool xlsx);
    QString destinationFolder() const;

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
    QLabel *m_recentFile, *m_recentMeta, *m_recentWhen;
    QPushButton* m_openFolder;
};

} // namespace phy104
