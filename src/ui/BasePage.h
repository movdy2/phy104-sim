#pragma once
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace phy104 {

class AppContext;

/// Shared page shell with an optional title/Save/Export header; embedded workspaces can omit it.
class BasePage : public QWidget {
    Q_OBJECT
public:
    BasePage(const QString& title, const QString& subtitle, AppContext* context, QWidget* parent = nullptr,
             bool showHeader = true);

    /// What the status bar shows while this page is open.
    struct StatusInfo {
        QString left = "Ready";
        QString mid = "Double precision";
        QString right = "Angles measured from normal";
        bool error = false;                   // red text (paused)
    };
    const StatusInfo& statusInfo() const { return m_status; }
    bool isPaused() const { return m_paused; }
    /// Kept for page compatibility; the shared Save + Export header is already applied by the base constructor.
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
    QPushButton* m_saveButton = nullptr;
    QToolButton* m_exportButton = nullptr;
    StatusInfo m_status;
    bool m_paused = false;
};

} // namespace phy104
