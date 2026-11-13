#pragma once
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace phy104 {

class AppContext;

/// Page with the Figma header: big title, subtitle, "SI UNITS · DEG" tag and a Save session button.
class BasePage : public QWidget {
    Q_OBJECT
public:
    BasePage(const QString& title, const QString& subtitle, AppContext* context, QWidget* parent = nullptr);

    /// What the status bar shows while this page is open.
    struct StatusInfo {
        QString left = "Ready";
        QString mid = "Double precision";
        QString right = "Angles measured from normal";
        bool error = false;                   // red text (paused)
    };
    const StatusInfo& statusInfo() const { return m_status; }
    bool isPaused() const { return m_paused; }
    void setUnitsTag(const QString& text);
    /// Header of the second Figma batch: "Save" (outline) instead of the units tag + "Save session".
    void useSaveExportHeader();
    /// The download icon of the header: one button, three formats (PDF report, Excel, CSV).
    QToolButton* exportButton() const { return m_exportButton; }

signals:
    void openPageRequested(int index);        // used by the dashboard
    void stateChanged();                      // status bar text or paused state changed
    /// The header export menu: 0 = PDF report, 1 = Excel (.xlsx), 2 = CSV (.csv).
    void exportFormatRequested(int format);

protected:
    /// Controls (fixed width) | centre (stretch) | right column (fixed width), below the header.
    struct Columns { QVBoxLayout* left; QVBoxLayout* center; QVBoxLayout* right; };
    Columns addThreeColumns(int leftWidth = 238, int rightWidth = 274);
    QVBoxLayout* content() const { return m_content; }

    void setStatusInfo(const StatusInfo& info, bool paused);

    AppContext* m_ctx;

private:
    QVBoxLayout* m_content;
    QLabel* m_units;
    QPushButton* m_saveButton = nullptr;
    QToolButton* m_exportButton = nullptr;
    StatusInfo m_status;
    bool m_paused = false;
};

} // namespace phy104
