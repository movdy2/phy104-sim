#pragma once
#include "BasePage.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace phy104 {

class MarkdownWorkspace;
class ModeTabs;
class NoticeCard;
class Panel;
class UnitField;

/// "Formulas, Notes & Export": formula reference | session notes + export preview | export settings.
class NotesExportPage : public BasePage {
    Q_OBJECT
public:
    explicit NotesExportPage(AppContext* context, QWidget* parent = nullptr);

private slots:
    void refreshSeries();
    void refreshPreview();
    void exportPrimary();
    void exportSecondary();
    void onFormatChanged();

private:
    const struct DataSeries* currentSeries() const;
    bool exportAs(bool xlsx);
    QString destinationFolder() const;

    // notes
    MarkdownWorkspace* m_notes;
    Panel* m_notesPanel;
    QLabel* m_linked;
    // export
    Panel* m_previewPanel;
    QTableWidget* m_table;
    QLabel* m_more;
    ModeTabs* m_format;
    QComboBox *m_seriesCombo, *m_folder;
    UnitField* m_fileName;
    QCheckBox *m_chkParams, *m_chkSteps, *m_chkData, *m_chkNotes;
    NoticeCard* m_ready;
    QPushButton *m_primary, *m_secondary;
};

} // namespace phy104
